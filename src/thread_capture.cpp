#include "util/assert.h"
#include "util/spinnaker.h"

#include "threads.h"

void configure(Spinnaker::CameraPtr camera) {
  auto &node_map = camera->GetNodeMap();
  Spinnaker::config(node_map, "TriggerMode", "Off");
  Spinnaker::config(node_map, "LineSelector", "Line3");
  Spinnaker::config(node_map, "LineMode", "Output");
  Spinnaker::config(node_map, "LineSource", "ExposureActive");
  Spinnaker::config(node_map, "AcquisitionMode", "Continuous");
  Spinnaker::config(node_map, "ExposureAuto", "Off");
  Spinnaker::config(node_map, "ExposureTime", 10000.0);
  Spinnaker::config(node_map, "PixelFormat", "BGR8");
  Spinnaker::config(node_map, "AdcBitDepth", "Bit10");

  auto &stream_node_map = camera->GetTLStreamNodeMap();
  Spinnaker::config(stream_node_map, "StreamBufferHandlingMode", "NewestOnly");
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
  std::cout << "[capture_thread] terminated." << std::endl;
}

} // namespace thread