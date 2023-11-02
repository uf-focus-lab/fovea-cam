#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h"
#include "util/vtconsole.h"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <thread>
// Create pipes
Threading::FastIO<cv::Mat> wide_view_pipe;
std::vector<Threading::FastIO<cv::Mat> *> fovea_pipes;
Threading::FIFO<context::MEMS_Position> pos_next;
Threading::FastIO<context::MEMS_Position> pos_back;

#define NO_THROW(STATEMENT)                                                    \
  try {                                                                        \
    STATEMENT;                                                                 \
  } catch (std::exception & e) {                                               \
    std::cerr << "[" << __func__ << "] " << e.what() << std::endl;             \
  } catch (...) {                                                              \
  }

void close_all_pipes(int) {
  std::cout << std::endl;
  thread::flag_exit = true;
  NO_THROW(wide_view_pipe.close());
  for (auto &pipe : fovea_pipes) {
    NO_THROW(pipe->close());
  }
  NO_THROW(pos_next.close());
  NO_THROW(pos_back.close());
  // Restore all signals to default
  signal(SIGINT, SIG_DFL);
  signal(SIGKILL, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}

int main(int argc, char **argv) {
  const auto task = argc > 1 ? std::string(argv[1]) : std::string{"move"};
  // Get env pointers
  thread::env.FRAMERATE = std::getenv("FPS");
  // Register signal handler
  signal(SIGINT, close_all_pipes);
  signal(SIGKILL, close_all_pipes);
  signal(SIGTERM, close_all_pipes);
  // Initialize serial port
  USB::SerialDevice mems(0x16c0, 0x0483);
  // Initialize cameras
  std::cout << "[main] Looking for spinnaker cameras." << std::endl;
  auto spinnaker = Spinnaker::System::GetInstance();
  auto camList = spinnaker->GetCameras();
  Spinnaker::CameraPtr wide_camera(nullptr), fovea_camera(nullptr);
  for (unsigned idx = 0; idx < camList.GetSize(); idx++) {
    auto camera = camList[idx];
    camera->Init();
    const auto model = std::string(camera->DeviceModelName().c_str());
    if (model.ends_with("BFS-U3-16S2C")) {
      // wide angle camera
      wide_camera = camera;
    } else if (model.ends_with("BFS-U3-16S2C-BD")) {
      // fovea camera
      fovea_camera = camera;
    }
    camera->DeInit();
  }
  if (wide_camera == nullptr || fovea_camera == nullptr) {
    std::cerr << "[main] Unable to find cameras." << std::endl;
    camList.Clear();
    spinnaker->ReleaseInstance();
    return -1;
  }
  // Begin acquisition
  std::vector<std::thread> threads;
  // Task specific threads
  if (task == "move") {
    // Multiplex 4 streams
    for (unsigned i = 0; i < 4; i++) {
      fovea_pipes.push_back(new Threading::FastIO<cv::Mat>());
    }
    // Send positions
    try {
      // Broadcast idle position
      pos_next.write(context::MEMS_Position(0, 0));
      pos_next.write(context::MEMS_Position(0, 0));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      double step = 1.0, pos = 0.0;
      while (true) {
        pos += step;
        if (pos >= 80.0)
          step = -1.0;
        if (step < 0.0 && pos <= 0.0)
          break;
        pos_next.write({pos, pos, 2});
        pos_next.write({-pos, pos, 1});
        pos_next.write({-pos, -pos, 3});
        pos_next.write({pos, -pos, 4});
      }
      // Broadcast idle position
      pos_next.write(context::MEMS_Position(0, 0));
      pos_next.write(context::MEMS_Position(0, 0));
      std::this_thread::sleep_for(std::chrono::seconds(1));
    } catch (Threading::END &) {
      // Normal termination
    }
    // Close position pipe upon fifo emptied
    NO_THROW(pos_next.close(true));
  } else if (task == "track") {
    // Create 1 stream for fovea
    fovea_pipes.push_back(new Threading::FastIO<cv::Mat>());
    // Create control pipes
    Threading::FIFO<std::vector<context::ArUcoInfo>> wide_aruco_pos_pipe,
        fovea_aruco_pos_pipe;
    // Aruco detection thread
    // threads.push_back(std::thread(
    //     [&]() { thread::aruco(wide_view_pipe, wide_aruco_pos_pipe); }));
    threads.push_back(std::thread(
        [&]() { thread::aruco(*fovea_pipes[0], fovea_aruco_pos_pipe); }));
    // Tracking thread
    threads.push_back(std::thread([&]() {
      thread::track_pid(fovea_aruco_pos_pipe, pos_next, pos_back);
    }));
  } else {
    std::cerr << "[main] Unknown task: " << task << std::endl;
    return -1;
  }
  // Display Thread
  threads.push_back(
      std::thread([&]() { thread::display(wide_view_pipe, fovea_pipes); }));
  // Capture Threads
  threads.push_back(
      std::thread([&]() { thread::capture(wide_camera, wide_view_pipe); }));
  threads.push_back(
      std::thread([&]() { thread::capture(fovea_camera, fovea_pipes); }));
  // MEMS Thread
  threads.push_back(
      std::thread([&]() { thread::mems(mems, pos_next, pos_back); }));
  // Wait for threads to terminate
  for (auto &thread : threads) {
    thread.join();
  }
  // Release resources
  NO_THROW(camList.Clear());
  NO_THROW(spinnaker->ReleaseInstance());
  std::cout << "[main] terminated." << std::endl;
  return 0;
}
