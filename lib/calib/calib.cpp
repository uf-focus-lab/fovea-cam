#include "calib.h"

// Preloaded calibration coefficients
static const struct {
  double x, y, xy, c;
} Cx = {4.85459339325059, -0.06452967035358549, 0.00016280733222939038,
        -63.032525040658314},
  Cy = {-0.13787596565316557, -3.435103383528066, 0.0017389896032312644,
        -64.05920615697792};

static inline double clamp(double val, double min, double max) {
  if (val < min)
    return min;
  if (val > max)
    return max;
  return val;
}

static const double SCALE = 4.65;

namespace calib {
cv::Rect roi(const double Vx, const double Vy, const int W, const int H) {
  const double Px = Cx.x * Vx + Cx.y * Vy + Cx.xy * Vx * Vy + Cx.c,
               Py = Cy.x * Vx + Cy.y * Vy + Cy.xy * Vx * Vy + Cy.c;
  const int x = (1.0 - 1.0 / Cx.x) * (double)(W) / 2.0 + Px,
            y = (1.0 + 1.0 / Cy.y) * (double)(H) / 2.0 + Py,
            w = (double)(W) / SCALE, h = (double)(H) / SCALE;
  return cv::Rect(clamp(x, 0, W - w), clamp(y, 0, H - h), w, h);
}

} // namespace calib
