#pragma once

#include <opencv2/core/types.hpp>
#include <opencv2/opencv.hpp>

namespace calib {

typedef struct {
  struct {
    double x, y, xy, c;
  } X, Y;
} Coeff;

extern Coeff VtoP, PtoV;

extern cv::Point2d shift;

cv::Point2d cvt(Coeff &C, cv::Point2d &p);
cv::Point2d cvt(Coeff &C, cv::Point2d &&p);

// Center x, y are scoped to [-1, 1]
cv::Rect roi(const cv::Point2d center, const cv::Size size, double zoom);

// Add black padding when roi bleeds out of bound
cv::Mat view(const cv::Mat &src, const cv::Rect &roi);

// Convert tile touch position to mems voltage
cv::Point2d pos2volt(cv::Point2d pos);
cv::Point2d volt2pos(cv::Point2d volt);
} // namespace calib