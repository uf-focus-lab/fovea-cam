#pragma once

#include "fcmp/fcmp.h"
#include "threading/fifo.h"
#include "util/time.h"

#include <iostream>
#include <mutex>

namespace mems {

class Position {
public:
  // Value range: [-v_bias, +v_bias]
  // Out-ranged values will be clipped to nearest boundary.
  double x, y;
  fcmp_field_pos field;
  Position(double x, double y, uint8_t tag = 0) : x(x), y(y) {
    field.tag = tag;
  }
};

extern long delay;

class SyncWindow {

private:
  struct {
    unsigned long open, close;
  } window;
  bool closed = false;

public:
  Position position;
  SyncWindow(Position pos = Position(0, 0, 0));
  // Called by mems recv upon arrival of next sync window.
  std::shared_ptr<SyncWindow> conclude(Position next);
  // Getter of the tag for this window
  uint16_t tag();
  /*
   * Check if given time falls within this sync window.
   */
  int test(const unsigned long time = Time::us(),
           const unsigned long offset = delay);
};

extern Threading::FIFO<std::shared_ptr<mems::SyncWindow>> sync;

} // namespace mems
