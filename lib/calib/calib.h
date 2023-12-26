#pragma once

#include <opencv2/opencv.hpp>

namespace calib {
cv::Rect roi(const double Vx, const double Vy, const int W, const int H);
}