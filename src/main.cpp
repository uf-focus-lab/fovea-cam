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
Threading::FIFO<context::MEMS_Position> pos_in;
Threading::FastIO<context::MEMS_Position> pos_out;

// Signal to kill all threads
bool flag_exit = false;

#define NO_THROW(STATEMENT)                                                    \
  try {                                                                        \
    STATEMENT;                                                                 \
  } catch (std::exception & e) {                                               \
    std::cerr << "[" << __func__ << "] " << e.what() << std::endl;             \
  } catch (...) {                                                              \
  }

void close_all_pipes(int) {
  std::cout << std::endl;
  flag_exit = true;
  NO_THROW(wide_view_pipe.close());
  for (auto &pipe : fovea_pipes) {
    NO_THROW(pipe->close());
  }
  NO_THROW(pos_in.close());
  NO_THROW(pos_out.close());
  // Restore all signals to default
  signal(SIGINT, SIG_DFL);
  signal(SIGKILL, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}

namespace thread {

ENV env;

}

int main() {
  // Get env pointers
  thread::env.FRAMERATE = std::getenv("FRAMERATE");
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
  // Multiplex 4 streams
  for (unsigned i = 0; i < 4; i++) {
    fovea_pipes.push_back(new Threading::FastIO<cv::Mat>());
  }
  // Begin acquisition
  std::vector<std::thread> thread_list;
  // Display Thread
  thread_list.push_back(
      std::thread([&]() { thread::display(wide_view_pipe, fovea_pipes); }));
  // Capture Threads
  thread_list.push_back(
      std::thread([&]() { thread::capture(wide_camera, wide_view_pipe); }));
  thread_list.push_back(
      std::thread([&]() { thread::capture(fovea_camera, fovea_pipes); }));
  // Stack Thread
  // thread_list.push_back(
  //     std::thread([&]() { thread::stack(img_pipe[2], img_pipe[1], 8); }));
  // MEMS Thread
  thread_list.push_back(
      std::thread([&]() { thread::mems(mems, pos_in, pos_out); }));
  // Send positions
  try {
    for (double offset = 80.0; offset > -80.0; offset -= 1) {
      // // for (double y = -60.0; y < 60.0; y += 1.0) {
      // // for (double x = 60.0; x > -60.0; x -= 1.0) {
      pos_in.write(context::MEMS_Position(offset, -offset * 0.75));  // 3
      pos_in.write(context::MEMS_Position(-offset, offset * 0.75));  // 2
      pos_in.write(context::MEMS_Position(offset, offset * 0.75));   // 1
      pos_in.write(context::MEMS_Position(-offset, -offset * 0.75)); // 4
    }
    // }
    // }
    pos_in.write(context::MEMS_Position(0, 0));
  } catch (Threading::END &) {
    // Normal termination
  }
  // Close position pipe upon fifo emptied
  NO_THROW(pos_in.close(true));
  // Wait for threads to terminate
  for (auto &thread : thread_list) {
    thread.join();
  }
  // Release resources
  NO_THROW(camList.Clear());
  NO_THROW(spinnaker->ReleaseInstance());
  std::cout << "[main] terminated." << std::endl;
  return 0;
}
