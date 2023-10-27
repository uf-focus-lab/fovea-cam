#include "graphics/canvas.h"
#include "threading/flushing_pipe.h"
#include "threads.h"
#include "util/spinnaker.h"
#include "util/vtconsole.h"
#include <chrono>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <ratio>
#include <signal.h>
#include <thread>
// Create pipes
Threading::FlushingPipe<cv::Mat> img_pipe[3];

void close_all_pipes(int) {
  std::cout << std::endl;
  for (unsigned i = 0; i < sizeof(img_pipe) / sizeof(*img_pipe); i++) {
    img_pipe[i].close();
  }
}

int main() {
  // Register signal handler
  signal(SIGINT, close_all_pipes);
  signal(SIGKILL, close_all_pipes);
  signal(SIGTERM, close_all_pipes);
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
