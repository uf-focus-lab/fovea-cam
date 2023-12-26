#include "global.h"
#include "tasks.h"
#include "threads.h"

using namespace global;

namespace tasks {

void track(std::vector<std::thread> &threads) {
  // Create 1 stream for fovea
  fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
  // Create control pipes
  const auto
      // Wide angle -> aruco position
      wide_aruco_pos_pipe =
          new Threading::FastIO<std::vector<context::ArUcoInfo>>,
      // Fovea -> aruco position
      fovea_aruco_pos_pipe =
          new Threading::FastIO<std::vector<context::ArUcoInfo>>;
  // Aruco detection thread
  threads.push_back(std::thread([&]() {
    thread::aruco(wide_capture_pipe, *wide_aruco_pos_pipe, false);
  }));
  threads.push_back(std::thread(
      [&]() { thread::aruco(*fovea_pipes[0], *fovea_aruco_pos_pipe, true); }));
  // Tracking thread
  threads.push_back(std::thread([&]() {
    thread::track_pid(*wide_aruco_pos_pipe, *fovea_aruco_pos_pipe, pos_next,
                      pos_back);
  }));
}

} // namespace tasks