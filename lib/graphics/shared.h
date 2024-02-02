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
    const int b = static_cast<int>(std::round(255.0 * B * level));             \
    const int g = static_cast<int>(std::round(255.0 * G * level));             \
    const int r = static_cast<int>(std::round(255.0 * R * level));             \
    const int a = static_cast<int>(std::round(255.0 * alpha));                 \
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

static inline double fmod_positive(double x, double y) {
  double result = std::fmod(x, y);
  return result >= 0. ? result : result + y;
}

// https://gist.github.com/ciembor/1494530
/*
 * Converts an HUE to r, g or b.
 * returns float in the set [0, 1].
 */
static inline float hue2rgb(float p, float q, float t) {
  t = fmod_positive(t, 1.0);
  if (t < 1. / 6)
    return p + (q - p) * 6 * t;
  else if (t < 1. / 2)
    return q;
  else if (t < 2. / 3)
    return p + (q - p) * (2. / 3 - t) * 6;
  else
    return p;
}

static inline const cv::Scalar hsla(double h, double s, double l,
                                    double a = 1.0) {
  h = fmod_positive(h, 1.0);
  s = clamp(s, 0.0, 1.0);
  l = clamp(l, 0.0, 1.0);
  a = clamp(a, 0.0, 1.0);
  if (s > 0.) {
    float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;
    return {
        std::round(hue2rgb(p, q, h - 1. / 3) * 255), // B
        std::round(hue2rgb(p, q, h) * 255),          // G
        std::round(hue2rgb(p, q, h + 1. / 3) * 255), // R
        std::round(a * 255)                          // A
    };
  } else {
    return {std::round(l * 255)};
  }
}

} // namespace color
