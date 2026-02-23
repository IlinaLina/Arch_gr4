#include "filters.h"
#include <algorithm>
#include <cmath>

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
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            cv::Vec3b pixel = img.at<cv::Vec3b>(y, x);
            for (int c = 0; c < 3; c++) {
                result.at<cv::Vec3b>(y, x)[c] = std::abs(255 - pixel[c]);
            }
        }
    }
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
            result.at<cv::Vec3b>(y, x)[0] = 255;
        }
    }
    return result;
}

cv::Mat all_green_filter(const cv::Mat& img) {
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            result.at<cv::Vec3b>(y, x)[1] = 255;
        }
    }
    return result;
}

cv::Mat all_blue_filter(const cv::Mat& img) {
    cv::Mat result = img.clone();
    for (int y = 0; y < img.rows; y++) {
        for (int x = 0; x < img.cols; x++) {
            result.at<cv::Vec3b>(y, x)[2] = 255;
        }
    }
    return result;
}