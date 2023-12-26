#include "mems/mems.h"

#include "util/assert.h"
#include "util/spinnaker.h"
#include "util/time.h"

#include "threads.h"

#include <mutex>

#undef LOGNAME
#define LOGNAME "[thread::capture]"

std::mutex configure_mutex;

void configure(Spinnaker::CameraPtr &camera,
               const bool is_zoom_camera = false) {
  camera->Init();
  std::lock_guard<std::mutex> lock(configure_mutex);
  std::cerr << LOGNAME " Setting up " << camera->DeviceModelName().c_str()
            << std::endl;
  { // Camera parameters
    auto map = Spinnaker::ConfigurableMap(camera->GetNodeMap());
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

    if (thread::env.FRAMERATE) {
      const auto env_rate = std::string(thread::env.FRAMERATE);
      const double rate = std::stod(env_rate);
      std::cerr << "[thread::capture] Setting framerate to " << rate
                << " (raw: " << env_rate << ")" << std::endl;
      ASSERT(rate > 0.0, "Invalid framerate");
      map.set("AcquisitionFrameRateEnable", true);
      map.set("AcquisitionFrameRate", rate);
    } else {
      map.set("AcquisitionFrameRateEnable", false);
    }

    double exp = thread::env.EXPOSURE ? std::stod(thread::env.EXPOSURE) * 1000.0
                                      : 1000.0;
    map.set("ExposureTime", is_zoom_camera ? exp * 30.0 : exp * 1.0);
    map.set("GainAuto", "Off");
    map.set("Gain", 0.0);
    // Image format
    map.set("PixelFormat", "BayerRG8");
    // Try and set ADC bit depth to 14, 12, 10, 8
    false ||                               //
        map.set("AdcBitDepth", "Bit14") || //
        map.set("AdcBitDepth", "Bit12") || //
        map.set("AdcBitDepth", "Bit10") || //
        map.set("AdcBitDepth", "Bit8");
  }
  { // Stream parameters
    auto map = Spinnaker::ConfigurableMap(camera->GetTLStreamNodeMap());
    // Only latest buffer is used, old buffers are dropped.
    map.set("StreamBufferHandlingMode", "NewestOnly");
  }
  // Get time offset relative to OS timestamp
  camera->BeginAcquisition();
}

namespace thread {
// Zoom camera
void capture(Spinnaker::CameraPtr &camera,
             std::vector<Threading::FastIO<cv::Mat> *> pipes_out,
             Threading::FastIO<mems::Position> &pos_real) {
  try {
    configure(camera, true);
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    for (auto &pipe : pipes_out) {
      pipe->close();
    }
    return;
  }
  // Capture loop
  try {
    std::shared_ptr<mems::SyncWindow> sync_window = mems::sync.read();
    while (1) {
      auto img_ptr = camera->GetNextImage();
      auto now = Time::us();
      // Find the matching sync window
      while (sync_window->test(now) > 0) {
        sync_window = mems::sync.read();
      }
      pos_real.write(sync_window->position);
      // Use the sync window to determine the position tag
      const auto tag = sync_window->tag();
      // std::cerr << LOGNAME " Got tag (" << tag << ")" << std::endl;
      if (tag == 0) {
        // BroadCast
        auto mat_ptr = std::make_shared<const cv::Mat>(
            Spinnaker::fromImagePtr(img_ptr, 1));
        for (auto &pipe : pipes_out)
          pipe->write(mat_ptr);
      } else if (tag <= pipes_out.size()) {
        // BroadCast
        auto mat_ptr = std::make_shared<const cv::Mat>(
            Spinnaker::fromImagePtr(img_ptr, 1));
        // Specific
        pipes_out[tag - 1]->write(mat_ptr);
      } else {
        std::cerr << LOGNAME "Invalid position tag: " << tag << std::endl;
      }
    }
  } catch (Threading::END &e) {
    // Normal termination
    for (auto &pipe : pipes_out) {
      pipe->close();
    }
    pos_real.close();
  } catch (Spinnaker::Exception &e) {
    std::cerr << LOGNAME "Spinnaker Error: " << e.what() << std::endl;
    for (auto &pipe : pipes_out) {
      pipe->close();
    }
    pos_real.close();
  }
  CATCH_ASSERT(;) catch (...) {
    std::cerr << LOGNAME "Unknown Error" << std::endl;
    for (auto &pipe : pipes_out) {
      pipe->close();
    }
    pos_real.close();
  };
  // Release camera instance
  try {
    camera->EndAcquisition();
    camera->DeInit();
  } catch (Spinnaker::Exception &e) {
    std::cerr << LOGNAME "Error: " << e.what() << std::endl;
  }
  std::cerr << LOGNAME " terminated." << std::endl;
}

void capture(Spinnaker::CameraPtr &camera,
             Threading::FastIO<cv::Mat> &pipe_out) {
  try {
    configure(camera);
  } catch (std::exception &e) {
    std::cerr << LOGNAME "Error: " << e.what() << std::endl;
    pipe_out.close();
    return;
  }
  // Capture loop
  try {
    while (1) {
      auto img_ptr = camera->GetNextImage();
      pipe_out.write(Spinnaker::fromImagePtr(img_ptr, 1));
    }
  } catch (Threading::END &e) {
    // Normal termination
  } catch (Spinnaker::Exception &e) {
    std::cerr << LOGNAME "Spinnaker Error: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << LOGNAME "Unknown Error" << std::endl;
    pipe_out.close();
  }
  // Release camera instance
  try {
    camera->EndAcquisition();
    camera->DeInit();
  } catch (Spinnaker::Exception &e) {
    std::cerr << LOGNAME "Error: " << e.what() << std::endl;
  }
  std::cerr << LOGNAME " terminated." << std::endl;
}

} // namespace thread