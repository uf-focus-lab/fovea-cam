#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "threading/fifo.h"
#include "threading/flushing_pipe.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h"
#include "util/vtconsole.h"

#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <thread>
// Create pipes
Threading::FlushingPipe<cv::Mat> img_pipe[3];
Threading::FIFO<context::mems_position> pos_in(1);
Threading::FlushingPipe<context::mems_position> pos_out;

// Signal to kill all threads
bool flag_exit = false;

#define NO_THROW(STATEMENT)                                                    \
  try {                                                                        \
    STATEMENT;                                                                 \
  } catch (...) {                                                              \
  }

void close_all_pipes(int) {
  std::cout << std::endl;
  flag_exit = true;
  for (unsigned i = 0; i < sizeof(img_pipe) / sizeof(*img_pipe); i++) {
    NO_THROW(img_pipe[i].close());
  }
  NO_THROW(pos_in.close());
  NO_THROW(pos_out.close());
  // Restore all signals to default
  signal(SIGINT, SIG_DFL);
  signal(SIGKILL, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}

int main() {
  // Register signal handler
  signal(SIGINT, close_all_pipes);
  signal(SIGKILL, close_all_pipes);
  signal(SIGTERM, close_all_pipes);
  // Initialize serial port
  USB::SerialDevice mems(0x16c0, 0x0483);
  // Initialize cameras
  auto spinnaker = Spinnaker::System::GetInstance();
  auto camList = spinnaker->GetCameras();
  if (camList.GetSize() < 2) {
    std::cerr << "No enough cameras (" << camList.GetSize()
              << " cameras found)." << std::endl;
    camList.Clear();
    spinnaker->ReleaseInstance();
    return -1;
  }
  // Begin acquisition
  std::vector<std::thread> thread_list;
  // Display Thread
  thread_list.push_back(
      std::thread([&]() { thread::display(img_pipe[0], img_pipe[1]); }));
  // Capture Threads
  thread_list.push_back(
      std::thread([&]() { thread::capture(camList[0], img_pipe[0]); }));
  thread_list.push_back(
      std::thread([&]() { thread::capture(camList[1], img_pipe[1]); }));
  // Stack Thread
  // thread_list.push_back(
  //     std::thread([&]() { thread::stack(img_pipe[2], img_pipe[1], 8); }));
  // MEMS Thread
  thread_list.push_back(
      std::thread([&]() { thread::mems(mems, pos_in, pos_out); }));
  // Send positions
  try {
    context::mems_position pos;
    float k = 1.0;
    for (double y = -60.0; y < 60.0; y += 1.0) {
      for (double x = 60.0; x > -60.0; x -= 1.0) {
        pos.x = x * k;
        pos.y = y;
        pos_in.write(pos);
      }
      k = -k;
    }
    pos.x = 0;
    pos.y = 0;
    pos_in.write(pos);
  } catch (Threading::Closed &) {
    // Normal termination
  }
  NO_THROW(pos_in.close());
  // Wait for threads to terminate
  for (auto &thread : thread_list) {
    thread.join();
  }
  // Release resources
  camList.Clear();
  spinnaker->ReleaseInstance();
  std::cout << "[main] terminated." << std::endl;
  return 0;
}
