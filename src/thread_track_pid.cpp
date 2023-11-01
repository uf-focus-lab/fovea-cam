#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"

// If the center of the marker is within this distance from the center of the
// image, no correction will be performed.
static const double max_output = 80.0, kp = 0.1;

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
    double volt_x = 0.0, volt_y = 0.0;
    while (!flag_exit) {
      // Read next frame from pipe
      auto info_vec = fovea_info_in.read();
      // Get latest position from mems
      auto current_pos = mems_pos_back.read();
      // Check if position is available
      if (!current_pos)
        continue;
      // Track only the first info
      if (info_vec.size() > 0) {
        auto &info = info_vec[0];
        // Apply correction and send to mems
        volt_x += kp * info.x;
        volt_y += kp * info.y;
        normalize(volt_x);
        normalize(volt_y);
        // Send to mems
        mems_pos_next.write({volt_x, volt_y});
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