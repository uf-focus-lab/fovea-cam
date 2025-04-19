#include "global.h"
#include "calib/calib.h"

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

#undef LOGNAME
#define LOGNAME "[global:context] "

void Context::close() {
  cap_wide.close();
  cap_fovea.close();
  mems_pos.close();
  mems_sync.close();
}

void Context::join() {
  for (auto &el : threads) {
    std::cerr << LOGNAME "Waiting for " << el.name << std::endl;
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

std::string ensure_config_dir() {
  const char *home = std::getenv("HOME");
  if (!home) {
    std::cerr << LOGNAME << "Warning: HOME environment variable is not set."
              << std::endl
              << "Using current working directory for config file:" << std::endl
              << (std::getenv("PWD") || "<UNKNOWN>") << "/config.env"
              << std::endl;
    return "./config.env";
  }
  std::filesystem::path config_dir = std::string(home) + "/.config/FoveaCam";
  if (!std::filesystem::exists(config_dir)) {
    std::error_code ec;
    if (!std::filesystem::create_directories(config_dir, ec)) {
      throw std::runtime_error("Failed to create config directory: " +
                               ec.message());
    }
  }
  return (config_dir / "config.env").string();
}

std::string config_file = ensure_config_dir();

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
        .gamma = 1.0, // Neutral gamma
        .black = 0.0, // Full dynamic range
    },
    // Fovea camera config
    {
        .fps = -1.0,  // No limit
        .exp = 33.33, // 30 fps
        .gain = 10.0, // 10 dB
        .gamma = 1.0, // Neutral gamma
        .black = 0.0, // Full dynamic range
    },
    // Motorized Lens config
    {
        .scale = 4.65, // x4.65
    }};

Config config(default_config);

void load_config() {
  std::ifstream env_file(config_file);
  if (!env_file) {
    std::cerr << LOGNAME "Unable to load config from " << config_file << ", "
              << "using default config" << std::endl;
    return;
  }
  std::string line;
  while (std::getline(env_file, line)) {
    false
        // Wide camera config
        || cfg(line, "WIDE.FPS", config.wide.fps)     //
        || cfg(line, "WIDE.EXP", config.wide.exp)     //
        || cfg(line, "WIDE.GAIN", config.wide.gain)   //
        || cfg(line, "WIDE.GAMMA", config.wide.gamma) //
        || cfg(line, "WIDE.BLACK", config.wide.black) //
        // Fovea camera config
        || cfg(line, "FOVEA.FPS", config.fovea.fps)     //
        || cfg(line, "FOVEA.EXP", config.fovea.exp)     //
        || cfg(line, "FOVEA.GAIN", config.fovea.gain)   //
        || cfg(line, "FOVEA.GAMMA", config.fovea.gamma) //
        || cfg(line, "FOVEA.BLACK", config.fovea.black) //
        // Motorized Lens config
        || cfg(line, "LENS.X", config.lens.x)         //
        || cfg(line, "LENS.Y", config.lens.y)         //
        || cfg(line, "LENS.Z", config.lens.z)         //
        || cfg(line, "LENS.SCALE", config.lens.scale) //
        // Calibration results
        || cfg(line, "CALIB.SHIFT.X", calib::shift.x) //
        || cfg(line, "CALIB.SHIFT.Y", calib::shift.y) //
        ;
  }
  std::cerr << LOGNAME "Config loaded from " << config_file << std::endl;
}

void save_config() {
  std::ofstream env_file(config_file);
  if (!env_file) {
    std::cerr << LOGNAME "Unable to save config to " << config_file
              << std::endl;
    return;
  }
  env_file << "WIDE.FPS=" << config.wide.fps << std::endl
           << "WIDE.EXP=" << config.wide.exp << std::endl
           << "WIDE.GAIN=" << config.wide.gain << std::endl
           << "WIDE.GAMMA=" << config.wide.gamma << std::endl
           << "WIDE.BLACK=" << config.wide.black << std::endl
           << "FOVEA.FPS=" << config.fovea.fps << std::endl
           << "FOVEA.EXP=" << config.fovea.exp << std::endl
           << "FOVEA.GAIN=" << config.fovea.gain << std::endl
           << "FOVEA.GAMMA=" << config.fovea.gamma << std::endl
           << "FOVEA.BLACK=" << config.fovea.black << std::endl
           << "LENS.X=" << config.lens.x << std::endl
           << "LENS.Y=" << config.lens.y << std::endl
           << "LENS.Z=" << config.lens.z << std::endl
           << "LENS.SCALE=" << config.lens.scale << std::endl
           << "CALIB.SHIFT.X=" << calib::shift.x << std::endl
           << "CALIB.SHIFT.Y=" << calib::shift.y << std::endl;
  std::cerr << LOGNAME "Config written to " << config_file << std::endl;
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

__FB__ *fb = nullptr;

void init_display() {
  std::cerr << "[init] Initializing display" << std::endl;

  // if (graphics::x11env() != 0) {
  //   std::cerr << "[init] Failed to set DISPLAY environment." << std::endl;
  //   std::exit(1);
  // }
  // fb = new graphics::X11FB();
  // if (!fb->is_open()) {
  //   std::cerr << "[init] Failed to open X11 framebuffer." << std::endl;
  //   std::exit(1);
  // }

  fb = new __FB__("FoveaCam Duo");
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
      } else if (model.ends_with("BFS-U3-28S5C-BD")) {
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
