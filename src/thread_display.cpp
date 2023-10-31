#include "graphics/canvas.h"
#include "threads.h"
#include "util/assert.h"
#include "util/vtconsole.h"
#include <memory>

void render(canvas::Canvas &canvas, std::shared_ptr<const cv::Mat> frame,
            cv::Rect tile) {
  cv::Mat tmp(frame->size(), frame->type());
  cv::flip(*frame, tmp, 1);
  canvas.show(tmp, tile);
}

namespace thread {

void display(Threading::FlushingPipe<cv::Mat> &pipe_tile_a,
             std::vector<Threading::FlushingPipe<cv::Mat> *> pipe_tile_b) {
  try {
    vtconsole::unbind_all();
    canvas::Canvas canvas("/dev/fb0", canvas::transform::NONE);
    canvas.clear();
    auto splash = cv::imread("assets/splash.png", cv::IMREAD_UNCHANGED);
    canvas.show(splash);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    canvas.clear();
    ASSERT(pipe_tile_b.size() <= 4, "Too much streams");
    // Pointers to previously rendered frames
    // avoids re-painting the same frame
    std::shared_ptr<const cv::Mat> wide_ptr = NULL;
    std::vector<std::shared_ptr<const cv::Mat>> fovea_ptrs;
    // Prepare display areas
    const unsigned w = canvas.shape().w, h = canvas.shape().h / 3;
    static const unsigned pad = 10;
    cv::Rect wide_view_tile = cv::Rect(pad, pad, w - (pad * 2), h - (pad * 2));
    std::vector<cv::Rect> fovea_tiles;
    for (unsigned int i = 0; i < pipe_tile_b.size(); i++) {
      const unsigned row = i / 2, col = i % 2;
      fovea_ptrs.push_back(NULL);
      fovea_tiles.push_back(cv::Rect(w * col + pad, h * (row + 1) + pad,
                                     w - (pad * 2), h - (pad * 2)));
    }
    try {
      while (1) {
        { // Wide angle
          auto next_ptr = pipe_tile_a.read();
          if (next_ptr == nullptr)
            continue; // No Data Available
          if (next_ptr == wide_ptr)
            continue; // Same Data
          // Update pointer and render to canvas
          wide_ptr = next_ptr;
          render(canvas, wide_ptr, wide_view_tile);
        }
        for (unsigned int i = 0; i < pipe_tile_b.size(); i++) { // Fovea streams
          auto next_ptr = pipe_tile_b[i]->read();
          if (next_ptr == nullptr)
            continue; // No Data Available
          if (next_ptr == fovea_ptrs[i])
            continue; // Same Data
          // Update pointer and render to canvas
          wide_ptr = next_ptr;
          render(canvas, wide_ptr, fovea_tiles[i]);
        }
      }
    } catch (Threading::Closed &e) {
      // Normal termination
    }
    CATCH_ASSERT(;);
    // Wait until other threads terminate
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Restore splash screen
    canvas.clear();
    canvas.show(splash);
  } catch (std::exception &e) {
    std::cerr << "[Thread::display] " << e.what() << std::endl;
  }
  pipe_tile_a.close();
  for (auto &pipe : pipe_tile_b)
    pipe->close();
  std::cout << "[Thread::display] terminated." << std::endl;
}

} // namespace thread