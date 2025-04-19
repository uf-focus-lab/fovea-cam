#pragma once

#include <cstdint>
#include <exception> // IWYU pragma: export
#include <opencv2/opencv.hpp>
#include <thread>

#define __FB__ graphics::SDL2FB
#define __FB_HEADER__ "graphics/sdl2fb.h"
#include __FB_HEADER__
#include "graphics/shared.h"
#include "mems/mems.h"
#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h" // IWYU pragma: keep

#define NO_THROW(STATEMENT)                                                    \
  try {                                                                        \
    STATEMENT;                                                                 \
  } catch (std::exception & e) {                                               \
    std::cerr << "[" << __func__ << "] " << e.what() << std::endl;             \
  } catch (...) {                                                              \
  }

namespace global {

typedef struct CamConfig {
  double fps;   // Frame per second, negative means no limit
  double exp;   // Exposure time in ms
  double gain;  // Gain (db)
  double gamma; // Gamma correction
  double black; // Black level (percentage)
  bool updated = false;
} CamConfig;

typedef struct LensConfig {
  double x, y, z; // 3 DOF Zoom Lens Configuration
  double scale;   // Zoom (scale) ratio, shoule be greater than 1.0
  bool updated = false;
} LensConfig;

typedef struct {
  CamConfig wide, fovea;
  LensConfig lens;
} Config;

extern const Config default_config;
extern Config config;
void load_config();
void save_config();

extern bool flag_back; // Flag to exit only to kiosk
extern bool flag_term; // Flag to terminate entirely
void init_signal();
void deinit_signal();

extern __FB__ *fb;
void init_display();

extern USB::SerialDevice *mems;
extern Spinnaker::SystemPtr spinnaker;
extern Spinnaker::CameraPtr wide_camera, fovea_camera;

void init_devices();
void deinit_devices();

typedef struct Fovea {
  std::uint16_t tag;
  double x, y;
  const cv::Mat mat;
  cv::Point2d volt() const { return {x / 180.0 + 0.5, y / 180.0 + 0.5}; }
} Fovea;

typedef struct {
  std::string name;
  std::thread thread;
} ThreadInfo;

} // namespace global

typedef threading::FIFO<mems::Position> PosFIFO;
typedef threading::FIFO<std::shared_ptr<mems::SyncWindow>> SyncFIFO;
typedef threading::FastIO<cv::Mat> MatPipe;
typedef threading::FastIO<global::Fovea> FoveaPipe;
typedef threading::FIFO<graphics::PointerEvent> PointerFIFO;

typedef struct Context {
  std::vector<global::ThreadInfo> threads;
  PosFIFO mems_pos;
  SyncFIFO mems_sync;
  MatPipe cap_wide;
  FoveaPipe cap_fovea;
  void close();
  void join();
} Context;
