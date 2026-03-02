#ifndef MANAGER_H
#define MANAGER_H

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <functional>
#include <atomic>
#include "filters.h"
#include "BlockingQueue.h"

namespace fs = std::filesystem;

struct FragmentInfo {
    cv::Mat fragment;
    int left_position;
};

class Manager {
private:
    std::string path;
    int processor_count;
    
    BlockingQueue<FragmentInfo*> task_queue;
    std::vector<FragmentInfo*> result_buffer;
    std::mutex result_mutex;
    
    cv::Mat current_img;
    int blur_strength = 5;
    int slice_size = 128;
    
    std::atomic<bool> all_tasks_added{false};

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