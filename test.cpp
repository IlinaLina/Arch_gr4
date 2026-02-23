#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <filesystem>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <functional>
#include <atomic>
#include <typeinfo>

namespace fs = std::filesystem;

// Фильтры (аналогичные filters.py)
cv::Mat blur_filter(const cv::Mat& img, int blur_strength) {
    int k = blur_strength / 2;
    cv::Mat result = img.clone();
    
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            int y_min = std::max(0, y - k);
            int y_max = std::min(img.rows, y + k + 1);
            int x_min = std::max(0, x - k);
            int x_max = std::min(img.cols, x + k + 1);
            
            cv::Rect roi(x_min, y_min, x_max - x_min, y_max - y_min);
            cv::Mat neighborhood = img(roi);
            
            cv::Scalar mean = cv::mean(neighborhood);
            result.at<cv::Vec3b>(y, x) = cv::Vec3b(mean[0], mean[1], mean[2]);
        }
    }
    return result;
}

cv::Mat invert_filter(const cv::Mat& img) {
    cv::Mat result;
    result = 255 - img;
    return result;
}

cv::Mat contrast_filter(const cv::Mat& img) {
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3b pixel = img.at<cv::Vec3b>(y, x);
            for (int c = 0; c < 3; c++) {
                result.at<cv::Vec3b>(y, x)[c] = std::abs(128 - pixel[c]);
            }
        }
    }
    return result;
}

cv::Mat all_red_filter(const cv::Mat& img) {
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            result.at<cv::Vec3b>(y, x)[0] = 255; // Blue channel
        }
    }
    return result;
}

cv::Mat all_green_filter(const cv::Mat& img) {
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            result.at<cv::Vec3b>(y, x)[1] = 255; // Green channel
        }
    }
    return result;
}

cv::Mat all_blue_filter(const cv::Mat& img) {
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            result.at<cv::Vec3b>(y, x)[2] = 255; // Red channel
        }
    }
    return result;
}

// Типы фильтров для идентификации
enum FilterType {
    FILTER_BLUR,
    FILTER_INVERT,
    FILTER_CONTRAST,
    FILTER_ALL_RED,
    FILTER_ALL_GREEN,
    FILTER_ALL_BLUE,
    FILTER_UNKNOWN
};

// Структура для фрагмента изображения
struct FragmentInfo {
    cv::Mat fragment;
    int left_position;
};

class Manager {
private:
    std::string path;
    int processor_count;
    
    std::queue<FragmentInfo*> task_buffer;
    std::vector<FragmentInfo*> result_buffer;
    std::mutex task_mutex;
    std::mutex result_mutex;
    std::condition_variable task_cv;
    
    cv::Mat current_img;
    int blur_strength = 7;
    int slice_size = 128;
    
    std::atomic<bool> slicer_finished{false};
    std::atomic<bool> stop_processing{false};

    // Функция для определения типа фильтра
    FilterType getFilterType(std::function<cv::Mat(const cv::Mat&)> filter) {
        // В C++ сложно получить имя функции, поэтому будем использовать
        // идентификацию по тому, как фильтр применяется
        // В данном случае мы не можем точно определить, но для blur нужна особая обработка
        // Поэтому будем передавать отдельный флаг
        return FILTER_UNKNOWN;
    }

public:
    Manager(int processor_count) : processor_count(processor_count) {}
    
    // Перегруженная функция run для разных типов фильтров
    void run(const std::string& img_path, const std::string& result_path, 
             std::function<cv::Mat(const cv::Mat&)> filter, bool isBlur = false) {
        this->path = img_path;
        
        for (const auto& entry : fs::directory_iterator(img_path)) {
            std::string img_name = entry.path().filename().string();
            std::string ext = entry.path().extension().string();
            
            // Проверка расширения
            if (ext != ".jpg" && ext != ".png" && ext != ".jpeg" && ext != ".bmp") {
                continue;
            }
            
            std::cout << "Processing: " << img_name << std::endl;
            
            // Очистка буферов
            clearBuffers();
            slicer_finished = false;
            stop_processing = false;
            
            // Загрузка изображения
            current_img = cv::imread(entry.path().string());
            if (current_img.empty()) {
                std::cerr << "Failed to load image: " << img_name << std::endl;
                continue;
            }
            cv::cvtColor(current_img, current_img, cv::COLOR_BGR2RGB);
            
            // Запуск потоков
            std::thread slicer_thread(&Manager::slicer, this, slice_size);
            
            std::vector<std::thread> threads;
            for (int i = 0; i < processor_count; i++) {
                threads.emplace_back(&Manager::processor, this, filter, isBlur);
            }
            
            slicer_thread.join();
            
            // Сигнал остановки для процессоров
            {
                std::lock_guard<std::mutex> lock(task_mutex);
                stop_processing = true;
                task_cv.notify_all();
            }
            
            for (auto& thread : threads) {
                thread.join();
            }
            
            // Сборка и сохранение результата
            if (!result_buffer.empty()) {
                // Сортировка по left_position
                std::sort(result_buffer.begin(), result_buffer.end(),
                          [](FragmentInfo* a, FragmentInfo* b) {
                              return a->left_position < b->left_position;
                          });
                
                cv::Mat result_img = collector();
                
                cv::cvtColor(result_img, result_img, cv::COLOR_RGB2BGR);
                
                // Создаем директорию для результатов, если её нет
                fs::create_directories(result_path);
                
                std::string output_path = result_path + "processed_" + img_name;
                cv::imwrite(output_path, result_img);
                std::cout << "Saved: " << output_path << std::endl;
            }
            
            // Очистка буфера результатов
            for (auto fragment : result_buffer) {
                delete fragment;
            }
            result_buffer.clear();
        }
    }
    
    // Перегруженная функция для blur фильтра
    void runBlur(const std::string& img_path, const std::string& result_path) {
        auto blurWrapper = [this](const cv::Mat& img) {
            return blur_filter(img, this->blur_strength);
        };
        run(img_path, result_path, blurWrapper, true);
    }
    
    void slicer(int slice_size) {
        int width = current_img.cols;
        int height = current_img.rows;
        
        int count_of_pieces = width / slice_size;
        
        for (int i = 0; i < count_of_pieces; i++) {
            int left = i * slice_size;
            int right = (i + 1) * slice_size;
            
            cv::Rect roi(left, 0, right - left, height);
            cv::Mat fragment = current_img(roi).clone();
            
            FragmentInfo* info = new FragmentInfo{fragment, left};
            
            {
                std::lock_guard<std::mutex> lock(task_mutex);
                task_buffer.push(info);
            }
            task_cv.notify_one();
        }
        
        if (width % slice_size != 0) {
            int left = count_of_pieces * slice_size;
            cv::Rect roi(left, 0, width - left, height);
            cv::Mat fragment = current_img(roi).clone();
            
            FragmentInfo* info = new FragmentInfo{fragment, left};
            
            {
                std::lock_guard<std::mutex> lock(task_mutex);
                task_buffer.push(info);
            }
            task_cv.notify_one();
        }
        
        slicer_finished = true;
    }
    
    void processor(std::function<cv::Mat(const cv::Mat&)> filter, bool isBlur) {
        while (true) {
            FragmentInfo* fragment_info = nullptr;
            
            {
                std::unique_lock<std::mutex> lock(task_mutex);
                task_cv.wait(lock, [this] {
                    return !task_buffer.empty() || stop_processing;
                });
                
                if (stop_processing && task_buffer.empty()) {
                    return;
                }
                
                if (!task_buffer.empty()) {
                    fragment_info = task_buffer.front();
                    task_buffer.pop();
                }
            }
            
            if (fragment_info) {
                cv::Mat processed_fragment;
                
                // Специальная обработка для blur
                if (isBlur) {
                    int k = blur_strength / 2;
                    int left = fragment_info->left_position;
                    int right = left + fragment_info->fragment.cols;
                    
                    // Расширенная область для размытия
                    int ext_left = std::max(0, left - k);
                    int ext_right = std::min(current_img.cols, right + k);
                    
                    cv::Rect extended_roi(
                        ext_left,
                        0,
                        ext_right - ext_left,
                        current_img.rows
                    );
                    
                    cv::Mat extended = current_img(extended_roi).clone();
                    cv::Mat blurred = blur_filter(extended, blur_strength);
                    
                    // Вырезаем центральную часть
                    int crop_left = left - ext_left;
                    cv::Rect crop_roi(crop_left, 0, fragment_info->fragment.cols, fragment_info->fragment.rows);
                    processed_fragment = blurred(crop_roi).clone();
                } else {
                    processed_fragment = filter(fragment_info->fragment);
                }
                
                FragmentInfo* result_info = new FragmentInfo{processed_fragment, fragment_info->left_position};
                
                {
                    std::lock_guard<std::mutex> lock(result_mutex);
                    result_buffer.push_back(result_info);
                }
                
                delete fragment_info;
            }
        }
    }
    
    cv::Mat collector() {
        if (result_buffer.size() == 1) {
            return result_buffer[0]->fragment;
        }
        
        int last_width = result_buffer.back()->fragment.cols;
        int slice_width = result_buffer[0]->fragment.cols;
        
        int width = slice_width * (result_buffer.size() - 1) + last_width;
        int height = result_buffer[0]->fragment.rows;
        
        cv::Mat result_img(height, width, CV_8UC3);
        
        for (auto frag_info : result_buffer) {
            cv::Rect roi(frag_info->left_position, 0, 
                        frag_info->fragment.cols, frag_info->fragment.rows);
            frag_info->fragment.copyTo(result_img(roi));
        }
        
        return result_img;
    }
    
    void clearBuffers() {
        std::lock_guard<std::mutex> lock(task_mutex);
        while (!task_buffer.empty()) {
            delete task_buffer.front();
            task_buffer.pop();
        }
    }
    
    void setBlurStrength(int strength) {
        blur_strength = strength;
    }
    
    void setSliceSize(int size) {
        slice_size = size;
    }
};

int main() {
    std::string abs_path = std::filesystem::current_path().string();
    std::string img_path = abs_path + "/test_images/";
    std::string res_path = abs_path + "/result_images/";
    
    fs::create_directories(img_path);
    fs::create_directories(res_path);
    
    std::cout << "Current path: " << abs_path << std::endl;
    std::cout << "Images path: " << img_path << std::endl;
    std::cout << "Results path: " << res_path << std::endl;
    
    Manager manager(4);
    
    // Тест с blur фильтром
    std::cout << "\n=== Testing Blur Filter ===" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    manager.runBlur(img_path, res_path);
    
    auto finish = std::chrono::high_resolution_clock::now();
    auto period = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
    std::cout << "Multithreaded processing time: " << period.count() / 1000.0 << " seconds" << std::endl;
    
    //Тест с инвертированием
    std::cout << "\n=== Testing Invert Filter ===" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    manager.run(img_path, res_path, invert_filter);
    
    finish = std::chrono::high_resolution_clock::now();
    period = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
    std::cout << "Multithreaded processing time: " << period.count() / 1000.0 << " seconds" << std::endl;
    
    // Одиночная обработка для сравнения
    std::cout << "\n=== Testing Single Image Processing ===" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    
    std::string test_img_path = abs_path + "/4k.jpg";
    cv::Mat img = cv::imread(test_img_path);
    if (!img.empty()) {
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
        blur_filter(img, 7);
        std::cout << "Single image processed" << std::endl;
    } else {
        std::cout << "Test image not found: " << test_img_path << std::endl;
    }
    
    finish = std::chrono::high_resolution_clock::now();
    period = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
    std::cout << "Single processing time: " << period.count() / 1000.0 << " seconds" << std::endl;
    
    return 0;
}