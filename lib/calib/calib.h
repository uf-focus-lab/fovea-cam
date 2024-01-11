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

cv::Point2d cvt(Coeff C, double x, double y);

// Center x, y are scoped to [-1, 1]
cv::Rect roi(const cv::Point2d center, const cv::Size size, double zoom);
} // namespace calib