#ifndef FILTERS_H
#define FILTERS_H

#include <opencv2/opencv.hpp>

cv::Mat blur_filter(const cv::Mat& img, int blur_strength);

cv::Mat invert_filter(const cv::Mat& img);

cv::Mat contrast_filter(const cv::Mat& img);

cv::Mat all_blue_filter(const cv::Mat& img);

cv::Mat all_green_filter(const cv::Mat& img);

cv::Mat all_red_filter(const cv::Mat& img);

#endif