#ifndef MANAGER_H
#define MANAGER_H

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
#include "filters.h"

namespace fs = std::filesystem;

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
    int blur_strength = 5;
    int slice_size = 128;
    
    std::atomic<bool> slicer_finished{false};
    std::atomic<bool> stop_processing{false};

public:
    Manager(int processor_count);
    
    void run(const std::string& img_path, const std::string& result_path, 
             std::function<cv::Mat(const cv::Mat&)> filter, bool isBlur = false);
    void runBlur(const std::string& img_path, const std::string& result_path);
    
    void slicer(int slice_size);
    void processor(std::function<cv::Mat(const cv::Mat&)> filter, bool isBlur);
    cv::Mat collector();
    
    void clearBuffers();
    void setBlurStrength(int strength);
    void setSliceSize(int size);
    void setProcessorNumber(int number);

    int getBlurStrength();
    int getSliceSize();
    int getProcessorNumber();

};

#endif