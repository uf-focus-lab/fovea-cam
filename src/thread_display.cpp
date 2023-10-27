#include "graphics/canvas.h"
#include "threads.h"
#include "util/vtconsole.h"

namespace thread {

void display(Threading::FlushingPipe<cv::Mat> &pipe_tile_a,
             Threading::FlushingPipe<cv::Mat> &pipe_tile_b) {
  vtconsole::unbind_all();
  canvas::Canvas canvas("/dev/fb0", canvas::transform::NONE);
  canvas.clear();
  auto splash = cv::imread("assets/splash.png", cv::IMREAD_UNCHANGED);
  canvas.show(splash);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  canvas.clear();
  // Prepare display areas
  // const unsigned int w = canvas.shape().w / 2, h = canvas.shape().h;
  // cv::Rect display_tile[2] = {cv::Rect(50, 50, w - 100, h - 100),
  //                             cv::Rect(w + 50, 50, w - 100, h - 100)};
  const unsigned int w = canvas.shape().w, h = canvas.shape().h / 2;
  cv::Rect display_tile[2] = {cv::Rect(50, 50, w - 100, h - 100),
                              cv::Rect(50, h + 50, w - 100, h - 100)};
  std::shared_ptr<const cv::Mat> mat_ptr[2] = {nullptr, nullptr};
  cv::Mat mat_tile[2];
  try {
    while (1) {
      for (unsigned i = 0; i < 2; i++) {
        auto next_ptr = (i == 0 ? pipe_tile_a : pipe_tile_b).read();
        if (next_ptr == nullptr)
          continue; // No Data Available
        if (next_ptr == mat_ptr[i])
          continue; // Same Data
        // Update pointer
        mat_ptr[i] = next_ptr;
        // Flip and show image
        cv::flip(*mat_ptr[i], mat_tile[i], 1);
        canvas.show(mat_tile[i], display_tile[i]);
      }
    }
  } catch (Threading::Closed &e) {
    // Normal termination
  }
  // Wait until other threads terminate
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  // Restore splash screen
  canvas.clear();
  canvas.show(splash);
  std::cout << "[Thread::display] terminated." << std::endl;
}

} // namespace thread