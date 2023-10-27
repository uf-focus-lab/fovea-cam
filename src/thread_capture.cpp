#include "threads.h"
#include "util/spinnaker.h"

namespace thread {

void capture(Spinnaker::CameraPtr camera,
             Threading::FlushingPipe<cv::Mat> &pipe_out) {
  try {
    camera->Init();
    camera->BeginAcquisition();
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    pipe_out.close();
    return;
  }
  // Capture loop
  try {
    while (1) {
      pipe_out.write(Spinnaker::fromImagePtr(camera->GetNextImage()));
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
  } catch (Spinnaker::Exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }
  std::cout << "[capture_thread] terminated." << std::endl;
}

} // namespace thread
