#include "global.h"

#include <csignal>
#include <fstream>
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

inline bool cfg(std::string &line, const char *key, double &value) {
  const std::size_t pos = line.find('=');
  if (pos == std::string::npos)
    return false;
  if (key != line.substr(0, pos))
    return false;
  const std::string raw_value = line.substr(pos + 1);
  try {
    value = std::stod(raw_value);
  } catch (std::invalid_argument &) {
    return false;
  }
  return true;
}

static const char *CONFIG_FILE = "/tmp/FoveaCam.env";

namespace global {

bool flag_back = false;
bool flag_term = false;

#undef LOGNAME
#define LOGNAME "[global:config] "

const Config default_config = {
    // Wide camera config
    {
        .fps = -1.0,  // No limit
        .exp = 16.66, // 60 fps
        .gain = 0.0,  // No gain
    },
    // Fovea camera config
    {
        .fps = -1.0,  // No limit
        .exp = 33.33, // 30 fps
        .gain = 10.0,
    },
    // Motorized Lens config
    {
        .scale = 4.65, // x4.65
    }};

Config config(default_config);

void load_config() {
  std::ifstream env_file(CONFIG_FILE);
  if (!env_file) {
    std::cerr << LOGNAME "Unable to load config from " << CONFIG_FILE << ", "
              << "using default config" << std::endl;
    return;
  }
  std::string line;
  while (std::getline(env_file, line)) {
    false
        // Wide camera config
        || cfg(line, "WIDE.FPS", config.wide.fps)   //
        || cfg(line, "WIDE.EXP", config.wide.exp)   //
        || cfg(line, "WIDE.GAIN", config.wide.gain) //
        // Fovea camera config
        || cfg(line, "FOVEA.FPS", config.fovea.fps)   //
        || cfg(line, "FOVEA.EXP", config.fovea.exp)   //
        || cfg(line, "FOVEA.GAIN", config.fovea.gain) //
        // Motorized Lens config
        || cfg(line, "LENS.X", config.lens.x)         //
        || cfg(line, "LENS.Y", config.lens.y)         //
        || cfg(line, "LENS.Z", config.lens.z)         //
        || cfg(line, "LENS.SCALE", config.lens.scale) //
        ;
  }
}

void save_config() {
  std::ofstream env_file(CONFIG_FILE);
  if (!env_file) {
    std::cerr << LOGNAME "Unable to save config to " << CONFIG_FILE
              << std::endl;
    return;
  }
  env_file << "WIDE.FPS=" << config.wide.fps << std::endl
           << "WIDE.EXP=" << config.wide.exp << std::endl
           << "WIDE.GAIN=" << config.wide.gain << std::endl
           << "FOVEA.FPS=" << config.fovea.fps << std::endl
           << "FOVEA.EXP=" << config.fovea.exp << std::endl
           << "FOVEA.GAIN=" << config.fovea.gain << std::endl
           << "LENS.X=" << config.lens.x << std::endl
           << "LENS.Y=" << config.lens.y << std::endl
           << "LENS.Z=" << config.lens.z << std::endl
           << "LENS.SCALE=" << config.lens.scale << std::endl;
}

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
