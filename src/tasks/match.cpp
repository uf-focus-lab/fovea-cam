#include "calib/calib.h"
#include "global.h"
#include "tasks.h"

using namespace global;

void fovea_view_matcher() {
  try {
    auto prev_fovea = fovea_pipes[1]->read();
    while (true) {
      // Get latest mems position
      auto pos = pos_real.read();
      if (pos == nullptr)
        continue;
      auto fovea = fovea_pipes[1]->read();
      if (fovea == prev_fovea || fovea == nullptr)
        continue;
      prev_fovea = fovea;
      // Get latest image
      auto wide = wide_capture_pipe.read();
      if (wide == nullptr)
        continue;
      // Calculate cropping region
      cv::Rect roi =
          calib::roi(pos->x, pos->y, wide->size().width, wide->size().height);
      // Push to pipe
      try {
        cv::Mat matched;
        cv::resize((*wide)(roi), matched, fovea->size());
        fovea_pipes[0]->write(std::move(matched));
      } catch (cv::Exception &e) {
        std::cerr << "[match] out of bound roi: " << roi << std::endl;
      }
    }
  } catch (Threading::END &) {
    // Normal termination
  }
  fovea_pipes[0]->close();
}

namespace tasks {

void match(std::vector<std::thread> &threads) {
  // Create 2 streams for fovea view
  // First being the real fovea image
  fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Second being the matched portion of wide angle image
  fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Thread to move the mems in a circular pattern
  threads.push_back(std::thread([&]() {
    try {
      // Broadcast idle position
      pos_next.write({0, 0, 2});
      static const double r = 80.0;
      double theta = 0.0;
      while (true) {
        theta += 0.02;
        if (theta >= 2 * M_PI)
          theta -= 2 * M_PI;
        const double x = r * cos(theta), y = r * sin(theta);
        pos_next.write({x, y, 2});
      }
    } catch (Threading::END &) {
      // Normal termination
    }
    // Close position pipe upon fifo emptied
    NO_THROW(pos_next.close(true));
  }));
  // Thread to push matched image to 2nd fovea pipe
  threads.push_back(std::thread(fovea_view_matcher));
}

} // namespace tasks