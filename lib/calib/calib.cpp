#include "calib.h"
#include "util/clamp.h"

using namespace calib;

// Preloaded calibration coefficients
Coeff calib::PtoV = {.X = {1.24970115, -0.01932751, -0.00301170, -0.11443390},
                     .Y = {0.07596387, -1.25011547, -0.17736825, 1.13141786}},
      calib::VtoP = {.X = {0.80072241, -0.01262265, 0.00028129, 0.10587980},
                     .Y = {-0.04858150, -0.78664337, 0.07905642, 0.89784833}};

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
