#include "calib.h"
#include "util/clamp.h"

using namespace calib;

static const double K1 = 2.4;
static const double C1 = 0.5 - 0.5 * K1;
static const double K2 = 1.0 / K1;
static const double C2 = 0.5 - 0.5 * K2;

// Preloaded calibration coefficients
Coeff calib::PtoV = {.X = {+0, K1, +0, C1}, .Y = {K1, -0, +0, C1}},
      calib::VtoP = {.X = {+0, K2, +0, C2}, .Y = {K2, +0, +0, C2}};

cv::Point2d calib::shift = {0.0, 0.0};

cv::Point2d calib::cvt(Coeff C, cv::Point2d p) {
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
