#pragma once

#include "util/clamp.h"
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

#define DEFINE_COLOR(COLOR, R, G, B)                                           \
  static inline const cv::Scalar COLOR(double level = 1.0,                     \
                                       double alpha = 1.0) {                   \
    level = clamp(level, 0.0, 1.0);                                            \
    alpha = clamp(alpha, 0.0, 1.0);                                            \
    const int b = static_cast<int>(255.0 * B * level);                         \
    const int g = static_cast<int>(255.0 * G * level);                         \
    const int r = static_cast<int>(255.0 * R * level);                         \
    const int a = static_cast<int>(255.0 * alpha);                             \
    return cv::Scalar_<int>(b, g, r, a);                                       \
  }

namespace color {

DEFINE_COLOR(mono, 1.0, 1.0, 1.0);

DEFINE_COLOR(red, 1.0, 0.0, 0.0);
DEFINE_COLOR(green, 0.0, 1.0, 0.0);
DEFINE_COLOR(blue, 0.0, 0.0, 1.0);

DEFINE_COLOR(yellow, 1.0, 1.0, 0.0);
DEFINE_COLOR(cyan, 0.0, 1.0, 1.0);
DEFINE_COLOR(magenta, 1.0, 0.0, 1.0);

} // namespace color
