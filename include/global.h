#pragma once

#include <cstdint>
#include <exception> // IWYU pragma: export
#include <opencv2/opencv.hpp>

#include "graphics/X11.h"
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

extern bool flag_term;
void init_signal();
void deinit_signal();

extern graphics::X11FB *fb;
void init_display();

extern USB::SerialDevice *mems;
extern Spinnaker::SystemPtr spinnaker;
extern Spinnaker::CameraPtr wide_camera, fovea_camera;

void init_devices();
void deinit_devices();

typedef struct {
  double fps, exp, gain, zoom;
} Config;

extern Config config;

typedef struct {
  std::uint16_t tag;
  double x, y;
  const cv::Mat mat;
} Fovea;

typedef struct {
  // ArUco marker ID embedded in the image
  int id;
  // Center Position of the detected marker
  std::vector<cv::Point2f> corners;
} ArUcoInfo;

typedef struct {
  std::string name;
  std::thread thread;
} ThreadInfo;

} // namespace global

typedef threading::FIFO<mems::Position> PosFIFO;
typedef threading::FIFO<std::shared_ptr<mems::SyncWindow>> SyncFIFO;
typedef threading::FastIO<cv::Mat> MatPipe;
typedef threading::FastIO<global::Fovea> FoveaPipe;

typedef struct Context {
  std::vector<global::ThreadInfo> threads;
  PosFIFO mems_pos;
  SyncFIFO mems_sync;
  MatPipe cap_wide;
  FoveaPipe cap_fovea;
  void close();
  void join();
} Context;

typedef threading::FastIO<std::vector<global::ArUcoInfo>> ArUcoPipe;