#include "canvas.hpp"
#include "spinnaker.hpp"
#include "vtconsole.hpp"
#include <iostream>
#include <opencv2/opencv.hpp>
#include <shared_mutex>
#include <signal.h>
#include <unistd.h>

canvas::Canvas *canvas_ptr = nullptr;
cv::Mat splash;

Spinnaker::SystemPtr spinnaker;
Spinnaker::CameraList camList;
std::vector<Spinnaker::CameraPtr> camera;

void releaseSpinnakerResources() {
  std::cerr << "[PID:" << getpid() << ", tid: " << gettid() << "] "
            << "Releasing Spinnaker resources" << std::endl;
  // Release resources
  for (auto cam : camera) {
    try {
      cam->EndAcquisition();
      cam->DeInit();
    } catch (const Spinnaker::Exception &e) {
      std::cout << "Error: " << e.what() << std::endl;
    }
  }
  try {
    spinnaker->ReleaseInstance();
  } catch (const Spinnaker::Exception &e) {
    std::cout << "Error: " << e.what() << std::endl;
  }
}

void exit_cb(int signal) {
  std::cerr << "[PID:" << getpid() << ", tid: " << gettid() << "] "
            << "Exit on signal " << signal << std::endl;
  if (canvas_ptr && !splash.empty()) {
    canvas_ptr->clear();
    canvas_ptr->show(splash);
  }
  releaseSpinnakerResources();
  std::cerr << "Will exit now" << std::endl;
  _exit(0);
}

int main() {
  auto time_start = std::chrono::high_resolution_clock::now();
  // Initialize framebuffer
  vtconsole::unbind_all();
  canvas::Canvas canvas("/dev/fb0", canvas::transform::NONE);
  canvas_ptr = &canvas;
  splash = cv::imread("lib/assets/splash.png", cv::IMREAD_UNCHANGED);
  canvas.show(splash);
  // Initialize cameras
  spinnaker = Spinnaker::System::GetInstance();
  camList = spinnaker->GetCameras();
  if (camList.GetSize() < 2) {
    camList.Clear();
    spinnaker->ReleaseInstance();
    std::cout << "No enough cameras." << std::endl;
    return 1;
  }
  Spinnaker::CameraPtr camera[2];
  for (unsigned int i = 0; i < 2; i++) {
    camera[i] = camList.GetByIndex(i);
    try {
      camera[i]->Init();
      camera[i]->BeginAcquisition();
    } catch (Spinnaker::Exception &e) {
      std::cout << "Error: " << e.what() << std::endl;
      return 1;
    }
  }
  // Register signal handler
  signal(SIGINT, exit_cb);
  signal(SIGQUIT, exit_cb);
  signal(SIGABRT, exit_cb);
  // Sleep until time_start + 2s
  auto time_now = std::chrono::high_resolution_clock::now();
  auto time_diff =
      std::chrono::duration_cast<std::chrono::seconds>(time_now - time_start)
          .count();
  usleep((time_diff < 2 ? 2 - time_diff : 0) * 1e6);
  // Acquire images
  const unsigned int w = canvas.shape().w / 2, h = canvas.shape().h;
  cv::Rect display_tile[2] = {cv::Rect(100, 100, w - 200, h - 200),
                              cv::Rect(w + 100, 100, w - 200, h - 200)};
  // std::shared_mutex lock[2];
  canvas.clear();
  std::vector<std::shared_mutex> lock(2);
  bool c = false;
  while (true) {
    c = !c;
    // Convert to OpenCV Mat
    for (unsigned int i = 1; i < 2; i++) {
      auto const image_ptr = camera[i]->GetNextImage();
      auto const image = Spinnaker::fromImagePtr(image_ptr);
      if (i == 0) {
        // Black and white level correction
        double min, max;
        cv::minMaxLoc(image, &min, &max, nullptr, nullptr);
        image.convertTo(image, -1, 255.0 / (max - min), -min);
      }
      if (c)
        continue;
      // if (canvas.fork() == 0) {
      //   if (!lock[i].try_lock()) {
      //     std::cout << "Lock not acquired" << std::endl;
      //     exit(0);
      //   }
      //   canvas.show(image, display_tile[i]);
      //   lock[i].unlock();
      //   _exit(0);
      // }
      canvas.show(image, display_tile[i], canvas::transform::FLIP_X);
    }
  }
  canvas.show(splash);
  releaseSpinnakerResources();
  return 0;
}
