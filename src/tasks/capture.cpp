#include "global.h"
#include "tasks.h"

#include <cmath>
#include <sstream>

#define LOG_NAME "[task::capture] "

void fovea_view_matcher();

using namespace global;

bool flag_start = false;

namespace tasks {

void capture(std::vector<std::thread> &threads) {
  // Create 2 streams for fovea view
  // First being the real fovea image
  fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Second being the matched portion of wide angle image
  fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Thread to move the mems in a circular pattern
  threads.push_back(std::thread([&]() {
    try {
      while (!flag_start)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // Scan in grid pattern
      for (double y = 90.0; y >= -90.0; y -= 10.0) {
        for (double x = -90.0; x <= 90.0; x += 10.0) {
          pos_next.write({x, y, 2});
        }
      }
      // Broadcast idle position
      pos_next.write({0, 0, 2});
    } catch (Threading::END &) {
      // Normal termination
    }
    // Close position pipe upon fifo emptied
    NO_THROW(pos_next.close(true));
    NO_THROW(pos_real.close());
  }));
  // Thread to push matched image to 2nd fovea pipe
  threads.push_back(std::thread(fovea_view_matcher));
  // Thread to capture from both cameras
  threads.push_back(std::thread([&]() {
    try {
      auto prev_fovea = fovea_pipes[1]->read();
      auto prev_pos = pos_real.read();
      struct {
        int x, y;
      } prev = {0, 0};
      flag_start = true;
      while (true) {
        // Get latest image
        auto wide = wide_capture_pipe.read();
        if (wide == nullptr)
          continue;
        auto fovea = fovea_pipes[1]->read();
        if (fovea == prev_fovea || fovea == nullptr)
          continue;
        // Get latest mems position
        auto pos = pos_real.read();
        if (pos == nullptr || pos == prev_pos)
          continue;
        // Get common filename
        std::stringstream ss;
        const int x = round(pos->x) + 90, y = round(pos->y) + 90;
        if (x == prev.x && y == prev.y)
          continue;
        else
          prev = {x, y};
        // Hex
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
           << x << '-' << std::setw(2) << y;
        // Save images
        cv::imwrite(ss.str() + "-W.png", *wide);
        cv::imwrite(ss.str() + "-F.png", *fovea);
        std::cerr << LOG_NAME << x << ", " << pos->x << ", " << y << ", "
                  << pos->y << ", " << ss.str() << std::endl;
        // Update previous
        prev_fovea = fovea;
        prev_pos = pos;
      }
    } catch (Threading::END &) {
      // Normal termination
    }
  }));
}

} // namespace tasks