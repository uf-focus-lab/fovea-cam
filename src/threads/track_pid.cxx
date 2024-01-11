#include "context.h"
#include "threads.h"

#include "util/assert.h"

#include <iostream>
#include <sstream>

#undef LOGNAME
#define LOGNAME "[threads::track_pid]"

// If the center of the marker is within this distance from the center of the
// image, no correction will be performed.
static const double max_output = 90.0, kp = -0.02;

void normalize(double &volt, double max = max_output) {
  if (volt > max)
    volt = max;
  else if (volt < -max)
    volt = -max;
}

void track_pid(ArUcoPipe &wide_info_in, ArUcoPipe &fovea_info_in,
               PosPipe &mems_pos_in, PosFIFO &mems_pos_out) {
  try {
    mems_pos_out.write({0.0, 0.0});
    mems_pos_out.write({0.0, 0.0});
    int prev_id = 0;
    std::shared_ptr<const std::vector<global::ArUcoInfo>> prev_info_fovea =
        nullptr;
    while (!global::flag_term) {
      // Read next frame from pipe
      auto info_wide = wide_info_in.read(), info_fovea = fovea_info_in.read();
      // Get latest position from mems
      auto current_pos = mems_pos_in.read();
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
      mems_pos_out.write({mems_x, mems_y});
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
  mems_pos_out.close();
  mems_pos_in.close();
  std::cerr << LOGNAME " terminated." << std::endl;
}

std::thread threads::track_pid(ArUcoPipe &wide_info_in, ArUcoPipe &fovea_info_in,
                              PosPipe &mems_pos_in, PosFIFO &mems_pos_out) {
  return std::thread([&]() {
    track_pid(wide_info_in, fovea_info_in, mems_pos_in, mems_pos_out);
  })
}
