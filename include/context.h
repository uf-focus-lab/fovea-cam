#pragma once

#include <opencv2/opencv.hpp>
#include <vector>

namespace context {

typedef struct {
  // ArUco marker ID embedded in the image
  int id;
  // Center Position of the detected marker
  std::vector<cv::Point2f> corners;
} ArUcoInfo;

} // namespace context
