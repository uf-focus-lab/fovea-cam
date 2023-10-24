#include "canvas.hpp"
#include "flushing_pipe.hpp"
#include "spinnaker.hpp"
#include "vtconsole.hpp"
#include <chrono>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <ratio>
#include <signal.h>
#include <thread>
// Create pipes
FlushingPipe::Pipe<cv::Mat> img_pipe[3] = {FlushingPipe::Pipe<cv::Mat>(),
                                           FlushingPipe::Pipe<cv::Mat>()};

void close_all_pipes(int) {
  std::cout << std::endl;
  img_pipe[0].close();
  img_pipe[1].close();
  img_pipe[2].close();
}

void capture_thread(Spinnaker::CameraPtr camera,
                    FlushingPipe::Pipe<cv::Mat> &img_pipe) {
  try {
    camera->Init();
    camera->BeginAcquisition();
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    img_pipe.close();
    return;
  }
  // Capture loop
  while (1) {
    auto const image = Spinnaker::fromImagePtr(camera->GetNextImage());
    try {
      img_pipe.write(image);
    } catch (FlushingPipe::PipeEnd &e) {
      break;
    }
  }
  // Release camera instance
  try {
    camera->EndAcquisition();
    camera->DeInit();
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  std::cout << "[capture_thread] terminated." << std::endl;
}

void stack_thread(FlushingPipe::Pipe<cv::Mat> &img_pipe_in,
                  FlushingPipe::Pipe<cv::Mat> &img_pipe_out, const size_t n) {
  size_t counter = 0;
  cv::Mat stack;
  while (1) {
    try {
      auto image = img_pipe_in.read();
      if (counter == 0) {
        image.convertTo(stack, CV_32FC4);
      } else {
        cv::add(stack, image, stack, cv::noArray(), CV_32FC4);
      }
      if (++counter >= n) {
        cv::Mat result(stack.size(), CV_8UC4);
        double minVal, maxVal;
        cv::minMaxLoc(stack, &minVal, &maxVal, NULL, NULL);
        stack -= minVal;
        cv::convertScaleAbs(stack, result, 1000.0 / (maxVal - minVal));
        img_pipe_out.write(std::move(result));
        counter = 0;
      }
    } catch (FlushingPipe::PipeEnd &e) {
      break;
    }
  }
  std::cout << "[stack_thread] terminated." << std::endl;
}

void display_thread(FlushingPipe::Pipe<cv::Mat> img_pipe[2]) {
  vtconsole::unbind_all();
  canvas::Canvas canvas("/dev/fb0", canvas::transform::NONE);
  canvas.clear();
  auto splash = cv::imread("lib/assets/splash.png", cv::IMREAD_UNCHANGED);
  canvas.show(splash);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  canvas.clear();
  // Prepare display areas
  // const unsigned int w = canvas.shape().w / 2, h = canvas.shape().h;
  // cv::Rect display_tile[2] = {cv::Rect(50, 50, w - 100, h - 100),
  //                             cv::Rect(w + 50, 50, w - 100, h - 100)};
  const unsigned int w = canvas.shape().w, h = canvas.shape().h / 2;
  cv::Rect display_tile[2] = {cv::Rect(50, 50, w - 100, h - 100),
                              cv::Rect(50, h + 50, w - 100, h - 100)};
  while (1) {
    try {
      for (unsigned i = 0; i < 2; i++) {
        cv::Mat image;
        cv::flip(img_pipe[i].read(), image, 1);
        canvas.show(image, display_tile[i]);
      }
    } catch (FlushingPipe::PipeEnd &e) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  // Wait until other threads terminate
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  // Restore splash screen
  canvas.clear();
  canvas.show(splash);
  std::cout << "[display_thread] terminated." << std::endl;
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
  std::thread display(display_thread, img_pipe);
  std::thread capture_0(
      [&]() { capture_thread(camList.GetByIndex(0), img_pipe[0]); });
  std::thread capture_1(
      [&]() { capture_thread(camList.GetByIndex(1), img_pipe[2]); });
  std::thread stack_1([&]() { stack_thread(img_pipe[2], img_pipe[1], 32); });
  // Wait for threads to terminate
  display.join();
  capture_0.join();
  capture_1.join();
  stack_1.join();
  // Release resources
  camList.Clear();
  spinnaker->ReleaseInstance();
  std::cout << "[main] terminated." << std::endl;
  return 0;
}
