#include "calib.h"
#include "util/clamp.h"

/*

Fitting from pixel offset to voltage
====================================

Vx(Px, Py) = 1.36552322 * Px
           + -0.00697848 * Py
           + 0.00864126 * Px * Py
           + -0.18335899

           + // {1.36552322, -0.00697848, 0.00864126, -0.18335899}

Vy(Px, Py) = 0.06833417 * Px
           + -1.35776728 * Py
           + -0.20080358 * Px * Py
           + 1.14426216

           + // {0.06833417, -1.35776728, -0.20080358, 1.14426216}

Fitting from voltage to pixel offset
====================================

Px(Vx, Vy) = 0.72828273 * Vx
           + -0.00305453 * Vy
           + 0.00336373 * Vx * Vy
           + 0.13789076 * 1

           + // {0.72828273, -0.00305453, 0.00336373, 0.13789076}

Py(Vx, Vy) = -0.04689634 * Vx
           + -0.72044447 * Vy
           + 0.06889148 * Vx * Vy
           + 0.83178949 * 1

           + // {-0.04689634, -0.72044447, 0.06889148, 0.83178949}

*/

using namespace calib;

// Preloaded calibration coefficients
Coeff calib::PtoV = {.X = {1.36552322, -0.00697848, 0.00864126, -0.18143269},
                     .Y = {0.06833417, -1.35776728, -0.20080358, 1.19491745}},
      calib::VtoP = {.X = {0.72828273, -0.00305453, 0.00336373, 0.13654497},
                     .Y = {-0.04689634, -0.72044447, 0.06889148, 0.86644754}};

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
