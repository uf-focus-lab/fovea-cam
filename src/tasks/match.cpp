#include "calib/calib.h"
#include "global.h"
#include "tasks.h"
#include <iostream>

void fovea_view_matcher() {
  try {
    auto prev_fovea = global::fovea_pipes[1]->read();
    while (true) {
      // Get latest mems position
      auto pos = global::pos_real.read();
      if (pos == nullptr)
        continue;
      auto fovea = global::fovea_pipes[1]->read();
      if (fovea == prev_fovea || fovea == nullptr)
        continue;
      prev_fovea = fovea;
      // Get latest image
      auto wide = global::wide_capture_pipe.read();
      if (wide == nullptr)
        continue;
      // Calculate cropping region
      cv::Rect roi =
          calib::roi(pos->x, pos->y, wide->size().width, wide->size().height);
      // Push to pipe
      try {
        cv::Mat matched;
        cv::resize((*wide)(roi), matched, fovea->size());
        global::fovea_pipes[0]->write(std::move(matched));
      } catch (cv::Exception &e) {
        std::cerr << "[match] out of bound roi: " << roi << std::endl;
      }
    }
  } catch (Threading::END &) {
    // Normal termination
  }
  global::pos_real.close();
  global::pos_next.close();
  global::fovea_pipes[0]->close();
  global::fovea_pipes[1]->close();
  std::cerr << "[task::match] terminated." << std::endl;
}

namespace tasks {

void match(std::vector<std::thread> &threads) {
  // Create 2 streams for fovea view
  // First being the real fovea image
  global::fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Second being the matched portion of wide angle image
  global::fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Thread to move the mems in a circular pattern
  threads.push_back(std::thread([&]() {
    try {
      // Broadcast idle position
      global::pos_next.write({0, 0, 2});
      static const double r = 80.0;
      double theta = 0.0;
      while (true) {
        theta += 0.02;
        if (theta >= 2 * M_PI)
          theta -= 2 * M_PI;
        const double x = r * cos(theta), y = r * sin(theta);
        global::pos_next.write({x, y, 2});
      }
    } catch (Threading::END &) {
      // Normal termination
    }
    // Close position pipe upon fifo emptied
    NO_THROW(global::pos_next.close());
    std::cerr << "[task::match::pos_out] terminated." << std::endl;
  }));
  // Thread to push matched image to 2nd fovea pipe
  threads.push_back(std::thread(fovea_view_matcher));
}

} // namespace tasks