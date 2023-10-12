#include "flushing_pipe.hpp"
#include "spinnaker.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>

FlushingPipe::Pipe<cv::Mat> img_pipe;

void capture_thread() {
  // Initialize cameras
  auto spinnaker = Spinnaker::System::GetInstance();
  auto camList = spinnaker->GetCameras();
  if (camList.GetSize() < 1) {
    camList.Clear();
    spinnaker->ReleaseInstance();
    std::cerr << "No enough cameras." << std::endl;
    img_pipe.close();
    return;
  }
  // Begin acquisition
  auto camera = camList.GetByIndex(0);
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
}

void display_thread() {
  while (1) {
    try {
      auto image = img_pipe.read();
      cv::imshow("image", image);
    } catch (FlushingPipe::PipeEnd &e) {
      break;
    }
  }
}

int main() {
  std::thread capture(capture_thread), display(display_thread);
  capture.join();
  display.join();
  return 0;
}
