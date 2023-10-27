#pragma once

namespace context {

typedef struct {
  // Value range: [-v_bias, +v_bias]
  // Out-ranged values will be clipped to nearest boundary.
  double x, y;
} mems_position;

} // namespace context
