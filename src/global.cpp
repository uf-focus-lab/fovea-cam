#include "global.h"

#include <csignal>
#include <glob.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <sys/wait.h>
#include <unistd.h>

void handle_signal(int) {
  std::cerr << std::endl
            << "Trying to exit gracefully, "
            << "press CTRL-C again to exit now." << std::endl;
  global::deinit_signal();
}

void Context::close() {
  cap_wide.close();
  cap_fovea.close();
  mems_pos.close();
  mems_sync.close();
}

void Context::join() {
  for (auto &el : threads) {
    std::cerr << "[global::context] Waiting for " << el.name << std::endl;
    el.thread.join();
  }
  threads.clear();
}

namespace global {

bool flag_term = false;

void init_signal() {
  flag_term = false;
  signal(SIGINT, handle_signal);
  signal(SIGKILL, handle_signal);
  signal(SIGTERM, handle_signal);
}

void deinit_signal() {
  flag_term = true;
  signal(SIGINT, SIG_DFL);
  signal(SIGKILL, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}

Config config = {
    .fps = 0.0,   // Frame per second, 0 means no limit
    .exp = 16.66, // Exposure time in ms
    .gain = 20.0, // Gain, only applies to fovea camera
    .zoom = 4.65  // Zoom ratio
};

graphics::X11FB *fb = nullptr;

void init_display() {
  std::cerr << "[init] Initializing display" << std::endl;

  if (graphics::x11env() != 0) {
    std::cerr << "[init] Failed to set DISPLAY environment." << std::endl;
    std::exit(1);
  }

  fb = new graphics::X11FB();

  if (!fb->is_open()) {
    std::cerr << "[init] Failed to open X11 framebuffer." << std::endl;
    std::exit(1);
  }
}

USB::SerialDevice *mems = nullptr;
Spinnaker::SystemPtr spinnaker = nullptr;
Spinnaker::CameraPtr wide_camera(nullptr);
Spinnaker::CameraPtr fovea_camera(nullptr);

void init_devices() {
  signal(SIGSEGV, [](int) {
    std::cerr << "[init] Segmentation fault, trying to exit gracefully."
              << std::endl;
    global::deinit_devices();
    std::exit(1);
  });
  // Initialize serial port
  if (mems == nullptr) {
    std::cerr << "[init] Looking for MEMS driver." << std::endl;
    mems = new USB::SerialDevice(0x16c0, 0x0483);
  }
  // Initialize cameras, must come after serial port initialization
  if (spinnaker == nullptr) {
    std::cerr << "[init] Initializing Spinnaker." << std::endl;
    spinnaker = Spinnaker::System::GetInstance();
    std::cerr << "[init] Looking for spinnaker cameras." << std::endl;
    auto camList = spinnaker->GetCameras();
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
      } else {
        camera->DeInit();
      }
    }
    camList.Clear();
  }
  // Check if cameras are found
  if (wide_camera == nullptr || fovea_camera == nullptr) {
    std::cerr << "[main] Unable to find cameras." << std::endl;
    deinit_devices();
    std::exit(1);
  }
}

void deinit_devices() {
  if (wide_camera != nullptr) {
    std::cerr << "[deinit] releasing wide camera resources." << std::endl;
    NO_THROW(wide_camera->DeInit());
    wide_camera = nullptr;
  }
  if (fovea_camera != nullptr) {
    std::cerr << "[deinit] releasing fovea camera resources." << std::endl;
    NO_THROW(fovea_camera->DeInit());
    fovea_camera = nullptr;
  }
  if (spinnaker != nullptr) {
    std::cerr << "[deinit] releasing spinnaker resources." << std::endl;
    NO_THROW(spinnaker->ReleaseInstance());
    spinnaker = nullptr;
  }
  if (mems != nullptr) {
    std::cerr << "[deinit] releasing mems resources." << std::endl;
    delete mems;
    mems = nullptr;
  }
}

} // namespace global
