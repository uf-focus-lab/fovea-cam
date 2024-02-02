#include "mems/mems.h"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

inline std::string ms_cap(const long us) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(2) << std::setw(6) << std::setfill(' ')
     << us / 1000.0;
  return ss.str();
}

#undef LOGNAME
#define LOGNAME "[mems::sync] "

namespace mems {

SyncWindow::SyncWindow(Position pos, long delay) : position(pos), delay(delay) {
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
int SyncWindow::test(const unsigned long time) {
  if (delay > 0 && (static_cast<long>(time) < delay)) {
    std::cerr << LOGNAME << "Time too early: " << time << std::endl;
    return -1;
  }
  const auto time_point = time - delay;
  const auto left = window.close, right = left + window.close - window.open;
  if (time_point < left) {
    // Sync window has not opened yet.
    std::cerr << LOGNAME << ms_cap(left - time_point) << " |<-" << std::endl;
    return -1;
  } else if (closed && time_point > right) {
    // Sync window has closed, and time point pasts ddl.
    // Time point belongs to a future sync window.
    // std::cerr << LOGNAME << "                          ->| "
    //           << ms_cap(time_point - right) << std::endl;
    return 1;
  } else {
    // if (closed) {
    //   std::cerr << LOGNAME "       |<- " << ms_cap(time_point - left) << ", "
    //             << ms_cap(right - time_point) << " ->|" << std::endl;
    // } else {
    //   std::cerr << LOGNAME "       |<- " << ms_cap(time_point - window.open)
    //             << ", ------ ->|" << std::endl;
    // }
    if (!closed) {
      window.open = time;
    }
    return 0;
  }
}

} // namespace mems
