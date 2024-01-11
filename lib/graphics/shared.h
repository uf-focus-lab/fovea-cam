#pragma once

#include <opencv2/opencv.hpp>

namespace graphics {

struct PointerEvent {
  bool valid;
  int x;
  int y;
  unsigned button_state, button_mask;
  bool is_down(unsigned int n) const { return (button_state & (1 << n)) != 0; }
  bool is_updated(unsigned int n) const {
    return (button_mask & (1 << n)) != 0;
  }
};

} // namespace graphics

namespace color {

static inline const cv::Scalar black(int alpha = 255) {
  return cv::Scalar(0, 0, 0, alpha);
}

static inline const cv::Scalar white(int alpha = 255) {
  return cv::Scalar(255, 255, 255, alpha);
}

static inline const cv::Scalar gray(int alpha = 255) {
  return cv::Scalar(128, 128, 128, alpha);
}

static inline const cv::Scalar red(int alpha = 255) {
  return cv::Scalar(0, 0, 255, alpha);
}

static inline const cv::Scalar green(int alpha = 255) {
  return cv::Scalar(0, 255, 0, alpha);
}

static inline const cv::Scalar blue(int alpha = 255) {
  return cv::Scalar(255, 0, 0, alpha);
}

static inline const cv::Scalar cyan(int alpha = 255) {
  return cv::Scalar(255, 128, 0, alpha);
}

static inline const cv::Scalar yellow(int alpha = 255) {
  return cv::Scalar(0, 255, 255, alpha);
}

static inline const cv::Scalar magenta(int alpha = 255) {
  return cv::Scalar(255, 0, 255, alpha);
}

} // namespace color