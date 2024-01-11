#include "mems/mems.h"

#include <string>

inline std::string ms_cap(const long us) {
  auto val = std::to_string((us / 1000) % 1000000);
  return std::string(6 - val.length(), ' ') + val;
}

namespace mems {

long delay = -5000; // us

SyncWindow::SyncWindow(Position pos) : position(pos) {
  window.open = Time::us();
}

std::shared_ptr<SyncWindow> SyncWindow::conclude(Position next) {
  auto sync = std::make_shared<SyncWindow>(SyncWindow(next));
  window.close = Time::us();
  closed = true;
  return sync;
};

uint16_t SyncWindow::tag() { return position.field.tag; }
/*
 * Check if given time falls within this sync window.
 * Recommended: use exposure time in us as offset
 */
int SyncWindow::test(const unsigned long time, const unsigned long offset) {
  if (delay > 0 && (signed)time < delay) {
    return -1;
  }
  const auto real_time = time - delay;
  if (real_time < window.open) {
    // std::cerr << "[mems::sync] " << ms_cap(window.open - real_time)
    //           << " ms ::|<-" << std::endl;
    // Sync window has not opened yet.
    return -1;
  } else if (closed && real_time > window.close) {
    // Sync window has closed, and we are past the close time.
    return 1;
  } else {
    // if ((real_time - window.open) > 10 * 1000) {
    // Log the abnormal delay
    // if (closed) {
    //   std::cerr << "[mems::sync] ::|<-" << ms_cap(real_time - window.open)
    //             << "ms : " << ms_cap(window.close - real_time)
    //             << "ms->|::" << std::endl;
    // } else {
    //   std::cerr << "[mems::sync] ::|<-" << ms_cap(real_time - window.open)
    //             << "ms : ******ms->|::" << std::endl;
    // }
    // }
    return 0;
  }
}

} // namespace mems