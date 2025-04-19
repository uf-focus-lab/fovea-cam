#include "calib.h"
#include "util/clamp.h"
#include <opencv2/core/types.hpp>

using namespace calib;

static const double K1 = 1.5;
static const double K2 = 1.0 / K1;

// Preloaded calibration coefficients
Coeff calib::PtoV = {.X = {-K1, 0, 0, 0}, .Y = {0, K1, 0, 0}},
      calib::VtoP = {.X = {-K2, 0, 0, 0}, .Y = {0, K2, 0, 0}};

cv::Point2d calib::shift = {0.0, 0.0};

cv::Point2d calib::cvt(Coeff &C, cv::Point2d &p) {
  double xy = p.x * p.y;
  const auto &CX = C.X, &CY = C.Y;
  return {
      CX.x * p.x + CX.y * p.y + CX.xy * xy + CX.c,
      CY.x * p.x + CY.y * p.y + CY.xy * xy + CY.c,
  };
}

cv::Point2d calib::cvt(Coeff &C, cv::Point2d &&p) {
  double xy = p.x * p.y;
  const auto &CX = C.X, &CY = C.Y;
  return {
      CX.x * p.x + CX.y * p.y + CX.xy * xy + CX.c,
      CY.x * p.x + CY.y * p.y + CY.xy * xy + CY.c,
  };
}

// [0, 1] -> [0, k * (1 - z)]
static inline int project(double r, double z, int k) {
  const double max = 1.0 - z;
  return static_cast<int>(clamp(r - 0.5 * z, 0.0, max) *
                          static_cast<double>(k));
}

cv::Rect calib::roi(const cv::Point2d C, const cv::Size S, double z) {
  if (z >= 1.0)
    z = 1.0 / z;
  const int W = S.width, H = S.height;
  const int x = project(C.x, z, W), y = project(C.y, z, H);
  int w = static_cast<int>(static_cast<double>(W) * z),
      h = static_cast<int>(static_cast<double>(H) * z);
  return cv::Rect(clamp(x, 0, W - w), clamp(y, 0, H - h), w, h);
}

cv::Mat calib::view(const cv::Mat &src, const cv::Rect &roi) {
  bool flag_bleed = false;
  cv::Rect bleed(roi);
  if (bleed.x < 0) {
    bleed.width += bleed.x;
    bleed.x = 0;
    flag_bleed = true;
  }
  if (bleed.y < 0) {
    bleed.height += bleed.y;
    bleed.y = 0;
    flag_bleed = true;
  }
  if (bleed.x + bleed.width > src.cols) {
    bleed.width = src.cols - bleed.x;
    flag_bleed = true;
  }
  if (bleed.y + bleed.height > src.rows) {
    bleed.height = src.rows - bleed.y;
    flag_bleed = true;
  }
  if (!flag_bleed) {
    return src(roi);
  } else {
    cv::Mat result(roi.height, roi.width, src.type(), cv::Scalar(0, 0, 0, 0));
    cv::Point offset(bleed.x - roi.x, bleed.y - roi.y);
    src(bleed).copyTo(result(cv::Rect(offset, bleed.size())));
    return result;
  }
}

cv::Point2d calib::pos2volt(cv::Point2d pos) {
  auto volt =
      calib::cvt(calib::PtoV, 2 * (pos - cv::Point2d{0.5, 0.5} - calib::shift));
  volt.x = clamp(volt.x, -1.0, +1.0);
  volt.y = clamp(volt.y, -1.0, +1.0);
  // std::cerr << "[calib] P " << pos << " >>> V " << volt << std::endl;
  return volt;
}

cv::Point2d calib::volt2pos(cv::Point2d volt) {
  auto pos = calib::cvt(calib::VtoP, volt) / 2.0 + cv::Point2d{0.5, 0.5} +
             calib::shift;
  // std::cerr << "[calib] P " << pos << " <<<< V " << volt << std::endl;
  return pos;
}
