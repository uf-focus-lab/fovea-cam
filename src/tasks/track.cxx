#include "global.h"
#include "tasks.h"
#include "threads.h"

using namespace global;

namespace tasks {

void track() {
  std::vector<std::thread> threads;
  // Create 1 stream for fovea
  fovea_pipes->push_back(new MatPipe);
  // Create control pipes
  const auto
      // Wide angle -> aruco position
      wide_aruco_pos_pipe =
          new ArUcoPipe,
      // Fovea -> aruco position
      fovea_aruco_pos_pipe =
          new ArUcoPipe;
  // Aruco detection thread
  threads.push_back(std::thread([&]() {
    threads::aruco(wide_capture_pipe, *wide_aruco_pos_pipe, false);
  }));
  threads.push_back(std::thread(
      [&]() { threads::aruco(*fovea_pipes[0], *fovea_aruco_pos_pipe, true); }));
  // Tracking thread
  threads.push_back(std::thread([&]() {
    threads::track_pid(wide_aruco_pos_pipe, fovea_aruco_pos_pipe, pos_next,
                      pos_back);
  }));
  // Wait for threads to terminate
  for (auto &t : threads)
    t.join();
  // Reclaim resources
  delete wide_aruco_pos_pipe;
  delete fovea_aruco_pos_pipe;
}

} // namespace tasks
