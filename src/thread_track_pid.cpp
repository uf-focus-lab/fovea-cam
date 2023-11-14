#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"

#include <iostream>
#include <sstream>

// If the center of the marker is within this distance from the center of the
// image, no correction will be performed.
static const double max_output = 90.0, kp = -0.02;

void normalize(double &volt, double max = max_output) {
  if (volt > max)
    volt = max;
  else if (volt < -max)
    volt = -max;
}

namespace thread {

#undef LOGNAME
#define LOGNAME "[thread::track_pid]"

void track_pid(
    Threading::FastIO<std::vector<context::ArUcoInfo>> &wide_info_in,
    Threading::FastIO<std::vector<context::ArUcoInfo>> &fovea_info_in,
    Threading::FIFO<context::MEMS_Position> &mems_pos_next,
    Threading::FastIO<context::MEMS_Position> &mems_pos_back) {
  try {
    mems_pos_next.write({0.0, 0.0});
    mems_pos_next.write({0.0, 0.0});
    int prev_id = 0;
    std::shared_ptr<const std::vector<context::ArUcoInfo>> prev_info_fovea =
        nullptr;
    while (!flag_exit) {
      // Read next frame from pipe
      auto info_wide = wide_info_in.read(), info_fovea = fovea_info_in.read();
      // Get latest position from mems
      auto current_pos = mems_pos_back.read();
      // Check if position is available
      if (current_pos == nullptr || info_wide == nullptr ||
          info_fovea == nullptr)
        continue;
      if (prev_info_fovea == info_fovea)
        continue;
      prev_info_fovea = info_fovea;
      if (info_fovea->size() == 0 || info_wide->size() == 0) {
        std::cerr << "fovea[" << info_fovea->size() << "] wide["
                  << info_wide->size() << "]" << std::endl;
        continue;
      }
      const auto &fovea = info_fovea->at(0), &wide = info_wide->at(0);
      // Get current mems position
      double mems_x = current_pos->x, mems_y = current_pos->y;
      // Apply correction and send to mems
      double x = 0, y = 0;
      for (const auto &p : fovea.corners) {
        x -= p.x;
        y += p.y;
      }
      x /= (double)fovea.corners.size();
      y /= (double)fovea.corners.size();
      mems_x += kp * x;
      mems_y += kp * y;
      normalize(mems_x);
      normalize(mems_y);
      // Send to mems
      mems_pos_next.write({mems_x, mems_y});
      // Check if the marker is the same
      if (prev_id == 0 && fovea.id > 0 && fovea.id == wide.id) {
        std::stringstream ss;
        ss << "@id; " << fovea.id << "; ";
        ss << "@volt; " << current_pos->x << ", " << current_pos->y << "; ";
        ss << "@wide; ";
        for (const auto &p : wide.corners)
          ss << p.x << ", " << p.y << "; ";
        ss << "@fovea; ";
        for (const auto &p : fovea.corners)
          ss << p.x << ", " << p.y << "; ";
        std::cout << ss.str() << std::endl;
        std::cerr << LOGNAME " Captured: " << fovea.id << std::endl;
      }
      prev_id = fovea.id;
    };
  } catch (Threading::END &e) {
    std::cerr << LOGNAME " PIPE END" << std::endl;
  } catch (std::exception &e) {
    std::cerr << LOGNAME " " << e.what() << std::endl;
  }
  wide_info_in.close();
  fovea_info_in.close();
  mems_pos_next.close();
  mems_pos_back.close();
  std::cerr << LOGNAME " terminated." << std::endl;
}

} // namespace thread