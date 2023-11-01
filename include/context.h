#pragma once

#include "fcmp/fcmp.h"

#include <cstdint>

namespace context {

class MEMS_Position {
public:
  // Value range: [-v_bias, +v_bias]
  // Out-ranged values will be clipped to nearest boundary.
  double x, y;
  fcmp_field_pos field;
  MEMS_Position(double x, double y, uint8_t tag = 0) : x(x), y(y) {
    field.tag = tag;
  }
};

} // namespace context
