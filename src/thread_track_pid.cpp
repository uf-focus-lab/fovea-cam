#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"

// If the center of the marker is within this distance from the center of the
// image, no correction will be performed.
static const double max_output = 90.0, kp = -0.04;

void normalize(double &volt, double max = max_output) {
  if (volt > max)
    volt = max;
  else if (volt < -max)
    volt = -max;
}

namespace thread {

#undef LOGNAME
#define LOGNAME "[thread::track_pid]"

void track_pid(Threading::FIFO<std::vector<context::ArUcoInfo>> &fovea_info_in,
               Threading::FIFO<context::MEMS_Position> &mems_pos_next,
               Threading::FastIO<context::MEMS_Position> &mems_pos_back) {
  try {
    mems_pos_next.write({0.0, 0.0});
    mems_pos_next.write({0.0, 0.0});
    while (!flag_exit) {
      // Read next frame from pipe
      auto info_vec = fovea_info_in.read();
      // Get latest position from mems
      auto current_pos = mems_pos_back.read();
      // Check if position is available
      if (current_pos == nullptr)
        continue;
      // Get current mems position
      double mems_x = current_pos->x, mems_y = current_pos->y;
      // Track only the first info
      if (info_vec.size() > 0) {
        auto &info = info_vec[0];
        // Apply correction and send to mems
        mems_x += kp * info.x;
        mems_y += kp * info.y;
        normalize(mems_x);
        normalize(mems_y);
        // Send to mems
        mems_pos_next.write({mems_x, mems_y});
      } else {
        std::cerr << LOGNAME " Got empty vector." << std::endl;
      }
    };
  } catch (Threading::END &e) {
  } catch (std::exception &e) {
    std::cerr << LOGNAME " " << e.what() << std::endl;
  }
  fovea_info_in.close();
  mems_pos_next.close();
  mems_pos_back.close();
  std::cout << LOGNAME " terminated." << std::endl;
}

} // namespace thread