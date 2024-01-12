#include "calib.h"
#include "util/clamp.h"

/*

Fitting from pixel offset to voltage
====================================

Vx(Px, Py) = 1.65351813 * Px
           + 0.02880365 * Py
           + -0.01224670 * Px * Py
           + -0.26734991

          // {1.65351813, 0.02880365, -0.01224670, -0.26734991}

Vy(Px, Py) = -0.21179073 * Px
           + 1.62766630 * Py
           + 0.26033701 * Px * Py
           + -0.38046607

          // {-0.21179073, 1.62766630, 0.26033701, -0.38046607}

Fitting from voltage to pixel offset
====================================

Px(Vx, Vy) = 0.60499259 * Vx
           + -0.00989779 * Vy
           + 0.00366316 * Vx * Vy
           + 0.15776422 * 1

          // {0.60499259, -0.00989779, 0.00366316, 0.15776422}

Py(Vx, Vy) = 0.04906417 * Vx
           + 0.59860207 * Vy
           + -0.05216969 * Vx * Vy
           + 0.24852338 * 1

          // {0.04906417, 0.59860207, -0.05216969, 0.24852338}

*/

using namespace calib;

// Preloaded calibration coefficients
Coeff calib::PtoV = {.X = {1.65351813, 0.02880365, -0.01224670, -0.26734991},
                     .Y = {-0.21179073, 1.62766630, 0.26033701, -0.38046607}},
      calib::VtoP = {.X = {0.60499259, -0.00989779, 0.00366316, 0.15776422},
                     .Y = {0.04906417, 0.59860207, -0.05216969, 0.24852338}};

cv::Point2d calib::shift = {0, 0};

cv::Point2d calib::cvt(Coeff C, double x, double y) {
  double xy = x * y;
  const auto &CX = C.X, &CY = C.Y;
  return {
      CX.x * x + CX.y * y + CX.xy * xy + CX.c,
      CY.x * x + CY.y * y + CY.xy * xy + CY.c,
  };
}

// [0, 1] -> [0, k * (1 - z)]
int project(double r, double z, int k) {
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
