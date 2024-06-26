#include "global.h"
#include "threads.h"

#include "util/assert.h"
#include "util/spinnaker.h"
#include "util/time.h"

#include <mutex>
#include <tuple>

#undef LOGNAME
#define LOGNAME "[threads:capture:configure] "

void configure(Spinnaker::ConfigurableMap &map,
               const global::CamConfig config) {
  if (config.fps >= 1.0) {
    map.set("AcquisitionFrameRateEnable", true);
    map.set("AcquisitionFrameRate", config.fps);
  } else {
    map.set("AcquisitionFrameRateEnable", false);
  }
  map.set("ExposureAuto", "Off");
  map.set("ExposureTime", config.exp * 1000.0);
  map.set("GainAuto", "Off");
  map.set("Gain", config.gain);
  if (std::abs(config.gamma - 1.0) <= 0.01) {
    map.set("GammaEnable", false);
  } else {
    map.set("GammaEnable", true);
    map.set("Gamma", config.gamma);
  }
  map.set("BlackLevelSelector", "All");
  map.set("BlackLevel", config.black);
}

// Only one thread can configure the camera at a time
std::mutex config_lock;

void configure(const Spinnaker::CameraPtr &camera,
               const global::CamConfig config, bool is_zoom_camera = false) {
  std::lock_guard<std::mutex> lock(config_lock);
  std::cerr << LOGNAME "Setting up " << camera->DeviceModelName().c_str()
            << std::endl;
  { // Camera parameters
    auto map = Spinnaker::ConfigurableMap(camera->GetNodeMap());
    // Enable chunk mode
    map.set("ChunkModeActive", true);
    // Disable trigger input
    map.set("TriggerMode", "Off");
    // Enable strobe output on line3 (zoom camera only)
    if (is_zoom_camera) {
      map.set("LineSelector", "Line3");
      map.set("LineMode", "Output");
      map.set("LineSource", "ExposureActive");
    }
    // Capture parameters
    map.set("AcquisitionMode", "Continuous");
    configure(map, config);
    // Image format
    map.set("PixelFormat", "BayerRG8");
    // Try and set ADC bit depth to 14, 12, 10, 8
    false ||                               //
        map.set("AdcBitDepth", "Bit12") || //
        map.set("AdcBitDepth", "Bit10") || //
        map.set("AdcBitDepth", "Bit8");
    // Disable auto white balance
    map.set("BalanceWhiteAuto", "Off");
  }
  { // Stream parameters
    auto map = Spinnaker::ConfigurableMap(camera->GetTLStreamNodeMap());
    // Only latest buffer is used, old buffers are dropped.
    map.set("StreamBufferHandlingMode", "NewestOnly");
  }
  try {
    camera->BeginAcquisition();
  } catch (Spinnaker::Exception &e) {
    std::cerr << LOGNAME "BeginAcquisition(): " << e.what() << std::endl;
  }
}

typedef threading::FIFO<std::tuple<Spinnaker::ImagePtr, unsigned long>> CapPipe;

#undef LOGNAME
#define LOGNAME "[threads:capture:readout] "

std::thread readout(const Spinnaker::CameraPtr &camera,
                    global::CamConfig &config, CapPipe &out) {
  return std::thread([&]() {
    try {
      auto map = Spinnaker::ConfigurableMap(camera->GetNodeMap());
      while (!global::flag_term) {
        if (config.updated) {
          configure(map, config);
          config.updated = false;
        }
        out.write({camera->GetNextImage(), Time::us()});
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    out.close();
    // Release camera instance
    try {
      std::cerr << LOGNAME "Stopping " +
                       std::string(camera->DeviceModelName().c_str()) + "\n";
      camera->EndAcquisition();
    } catch (Spinnaker::Exception &e) {
      std::cerr << LOGNAME "Error: " << e.what() << std::endl;
    }
  });
};

#undef LOGNAME
#define LOGNAME "[threads:capture:wide] "

std::thread threads::capture_wide(Context &ctx) {
  return std::thread([&]() {
    auto &out = ctx.cap_wide;
    const auto &camera = global::wide_camera;
    try {
      configure(camera, global::config.wide, false);
    } catch (std::exception &e) {
      std::cerr << LOGNAME "Error: " << e.what() << std::endl;
      out.close();
      return;
    }
    CapPipe cap;
    auto reader = readout(camera, global::config.wide, cap);
    try {
      while (!global::flag_term) {
        auto tuple = cap.read();
        auto img_ptr = std::get<0>(tuple);
        auto mat = Spinnaker::fromImagePtr(img_ptr, 1);
        out.write(mat);
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    cap.close();
    ctx.close();
    reader.join();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[threads:capture:fovea]"

// Fovea camera
std::thread threads::capture_fovea(Context &ctx) {
  return std::thread([&]() {
    auto &sync = ctx.mems_sync;
    auto &out = ctx.cap_fovea;
    const auto &camera = global::fovea_camera;
    try {
      configure(camera, global::config.fovea, true);
    } catch (std::exception &e) {
      std::cerr << "Error: " << e.what() << std::endl;
      out.close();
      return;
    }
    CapPipe cap;
    auto reader = readout(camera, global::config.fovea, cap);
    try {
      auto sync_window = sync.read();
      while (!global::flag_term) {
        auto tuple = cap.read();
        auto img_ptr = std::get<0>(tuple);
        auto ts = std::get<1>(tuple);
        auto mat = Spinnaker::fromImagePtr(img_ptr, 1);
        // Find the matching sync window
        while (1) {
          if (sync_window->test(ts) <= 0)
            break;
          if (sync.empty())
            break;
          if (global::flag_term)
            break;
          sync_window = sync.read();
        }
        // Construct and broadcast next fovea frame
        out.write({.tag = sync_window->tag(),
                   .x = sync_window->position.x,
                   .y = sync_window->position.y,
                   .mat = mat});
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    ctx.close();
    cap.close();
    reader.join();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}
