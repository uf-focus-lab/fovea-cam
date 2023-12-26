#include "context.h"
#include "global.h"
#include "tasks.h"
#include "threads.h"

#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "usb/serial_device.h"
#include "util/vtconsole.h"

#include <cstdlib>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <thread>

// Create pipes
Threading::FastIO<cv::Mat> global::wide_capture_pipe;
std::vector<Threading::FastIO<cv::Mat> *> global::fovea_pipes;
Threading::FIFO<mems::Position> global::pos_next(1);
Threading::FastIO<mems::Position> global::pos_back;
Threading::FastIO<mems::Position> global::pos_real;
using namespace global;

void close_all_pipes(int) {
  std::cerr << std::endl;
  thread::flag_exit = true;
  NO_THROW(wide_capture_pipe.close());
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
  thread::env.EXPOSURE = std::getenv("EXP");
  // Register signal handler
  signal(SIGINT, close_all_pipes);
  signal(SIGKILL, close_all_pipes);
  signal(SIGTERM, close_all_pipes);
  // Initialize serial port
  USB::SerialDevice mems(0x16c0, 0x0483);
  // Initialize cameras
  std::cerr << "[main] Looking for spinnaker cameras." << std::endl;
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
    tasks::move(threads);
  } else if (task == "track") {
    tasks::track(threads);
  } else if (task == "match") {
    tasks::match(threads);
  } else if (task == "capture") {
    tasks::capture(threads);
  } else {
    std::cerr << "[main] Unknown task: " << task << std::endl;
    return -1;
  }
  // Display Thread
  threads.push_back(
      std::thread([&]() { thread::display(wide_capture_pipe, fovea_pipes); }));
  // Capture Threads
  threads.push_back(
      std::thread([&]() { thread::capture(wide_camera, wide_capture_pipe); }));
  threads.push_back(std::thread(
      [&]() { thread::capture(fovea_camera, fovea_pipes, pos_real); }));
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
  std::cerr << "[main] terminated." << std::endl;
  return 0;
}
