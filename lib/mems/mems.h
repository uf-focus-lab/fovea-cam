#pragma once

#include "threading/fifo.h"
#include "util/time.h"

#include <iostream>
#include <mutex>

namespace mems {

extern long delay;

class SyncWindow {

private:
  struct {
    unsigned long open, close;
  } window;
  bool closed = false;
  uint16_t _tag;

public:
  SyncWindow(uint16_t tag);
  // Called by mems recv upon arrival of next sync window.
  std::shared_ptr<SyncWindow> conclude(uint16_t tag);
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
