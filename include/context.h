#pragma once

namespace context {

class MEMS_Position {
public:
  // Value range: [-v_bias, +v_bias]
  // Out-ranged values will be clipped to nearest boundary.
  double x, y;
  MEMS_Position(double x, double y) : x(x), y(y) {}
};

} // namespace context
