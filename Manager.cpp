#include "Manager.h"

Manager::Manager(int processor_count) : processor_count(processor_count) {}

void Manager::run(const std::string& img_path, const std::string& result_path, 
                  std::function<cv::Mat(const cv::Mat&)> filter, bool isBlur) {
    this->path = img_path;

    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& entry : fs::directory_iterator(img_path)) {
        std::string img_name = entry.path().filename().string();
        std::string ext = entry.path().extension().string();
        
        if (ext != ".jpg" && ext != ".png" && ext != ".jpeg" && ext != ".bmp") {
            continue;
        }

        clearBuffers();
        slicer_finished = false;
        stop_processing = false;
        
        current_img = cv::imread(entry.path().string());
        if (current_img.empty()) {
            std::cerr << "Failed to load image: " << img_name << std::endl;
            continue;
        }
        cv::cvtColor(current_img, current_img, cv::COLOR_BGR2RGB);
        
        std::thread slicer_thread(&Manager::slicer, this, slice_size);
        
        std::vector<std::thread> threads;
        for (int i = 0; i < processor_count; i++) {
            threads.emplace_back(&Manager::processor, this, filter, isBlur);
        }
        
        slicer_thread.join();
        
        {
            std::lock_guard<std::mutex> lock(task_mutex);
            stop_processing = true;
            task_cv.notify_all();
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        if (!result_buffer.empty()) {
            std::sort(result_buffer.begin(), result_buffer.end(),
                      [](FragmentInfo* a, FragmentInfo* b) {
                          return a->left_position < b->left_position;
                      });
            
            cv::Mat result_img = collector();
            
            cv::cvtColor(result_img, result_img, cv::COLOR_RGB2BGR);
            
            fs::create_directories(result_path);
            
            std::string output_path = result_path + "processed_" + img_name;
            cv::imwrite(output_path, result_img);
        }
        

        for (auto frag : result_buffer) {
            delete frag;
        }
        result_buffer.clear();
    }

    auto finish = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start);
    std::cout << "All images processed. It takes: " << total_time.count() / 1000.0 << " seconds" << std::endl;
    std::cout << std::endl;
}

void Manager::runBlur(const std::string& img_path, const std::string& result_path) {
    auto blurWrapper = [this](const cv::Mat& img) {
        return blur_filter(img, this->blur_strength);
    };
    run(img_path, result_path, blurWrapper, true);
}

void Manager::slicer(int slice_size) {
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

void Manager::processor(std::function<cv::Mat(const cv::Mat&)> filter, bool isBlur) {
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

            if (isBlur) {
                int k = blur_strength / 2;
                int left = fragment_info->left_position;
                int right = left + fragment_info->fragment.cols;
                
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

cv::Mat Manager::collector() {
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

void Manager::clearBuffers() {
    std::lock_guard<std::mutex> lock(task_mutex);
    while (!task_buffer.empty()) {
        delete task_buffer.front();
        task_buffer.pop();
    }
}

int Manager::getBlurStrength() {
    return this->blur_strength;
}

int Manager::getSliceSize() {
    return this->slice_size;
}

int Manager::getProcessorNumber() {
    return this->processor_count;
}

void Manager::setBlurStrength(int strength) {
    blur_strength = strength;
}

void Manager::setSliceSize(int size) {
    slice_size = size;
}

void Manager::setProcessorNumber(int number) {
    processor_count = number;
}