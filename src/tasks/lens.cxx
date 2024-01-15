#include "global.h"
#include "tasks.h"

using namespace global;

namespace tasks {

void move() {
  std::vector<std::thread> threads;
  // Multiplex 4 streams
  for (unsigned i = 0; i < 4; i++) {
    fovea_pipes->push_back(new MatPipe);
  }
  // Send positions in new thread
  threads.push_back(std::thread([&]() {
    try {
      // Broadcast idle position
      pos_next->write({0, 0});
      pos_next->write({0, 0});
      std::this_thread::sleep_for(std::chrono::seconds(1));
      double step = 1.0, pos = 0.0;
      while (!global::flag_term) {
        pos += step;
        if (pos >= 80.0)
          step = -1.0;
        if (step < 0.0 && pos <= 0.0)
          break;
        pos_next->write({pos, pos, 2});
        pos_next->write({-pos, pos, 1});
        pos_next->write({-pos, -pos, 3});
        pos_next->write({pos, -pos, 4});
      }
      // Broadcast idle position
      pos_next->write({0, 0});
      pos_next->write({0, 0});
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (threading::END &) {
      // Normal termination
    }
    // Close position pipe upon fifo emptied
    NO_THROW(pos_next->close(true));
  }));
};
} // namespace tasks
