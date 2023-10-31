#include "util/assert.h"
#include "util/spinnaker.h"

#include "threads.h"

#include <mutex>

std::mutex mtx;

void configure(Spinnaker::CameraPtr camera) {
  std::lock_guard<std::mutex> lock(mtx);
  const auto model = std::string(camera->DeviceModelName().c_str());
  const auto is_zoom_camera = model.ends_with("BFS-U3-16S2C-BD");
  std::cout << "[Thread::capture] Setting Up Camera \"" << model << "\""
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
    map.set("AcquisitionFrameRateEnable", false);
    // map.set("AcquisitionFrameRate", 60.0);
    map.set("ExposureAuto", "Off");
    map.set("ExposureTime", is_zoom_camera ? 10.0 * 1000.0 : 1000.0);
    map.set("GainAuto", "Off");
    map.set("Gain", is_zoom_camera ? 0.0 : 0.0);
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
}

namespace thread {

void capture(Spinnaker::CameraPtr camera,
             Threading::FlushingPipe<cv::Mat> &pipe_out) {
  try {
    camera->Init();
    configure(camera);
    camera->BeginAcquisition();
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    pipe_out.close();
    return;
  }
  // Capture loop
  try {
    while (1) {
      auto img_ptr = camera->GetNextImage();
      // auto timestamp = img_ptr->GetTimeStamp();
      pipe_out.write(Spinnaker::fromImagePtr(img_ptr));
    }
  } catch (Threading::Closed &e) {
    // Normal termination
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Spinnaker Error: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown Error" << std::endl;
  }
  // Release camera instance
  try {
    camera->EndAcquisition();
    camera->DeInit();
    camera = nullptr;
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  std::cout << "[Thread::capture] terminated." << std::endl;
}

} // namespace thread