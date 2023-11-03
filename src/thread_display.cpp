#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"
#include "util/vtconsole.h"
#include <memory>

namespace thread {

void display(Threading::FastIO<cv::Mat> &pipe_tile_a,
             std::vector<Threading::FastIO<cv::Mat> *> pipe_tile_b) {
  try {
    vtconsole::unbind_all();
    canvas::Canvas canvas("/dev/fb0", canvas::transform::NONE);
    auto splash = cv::imread("assets/splash.png", cv::IMREAD_UNCHANGED);
    canvas.clear().show(splash).apply();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    canvas.clear();
    ASSERT(pipe_tile_b.size() <= 4, "Too many streams");
    // Pointers to previously rendered frames
    // avoids re-painting the same frame
    std::shared_ptr<const cv::Mat> wide_ptr = nullptr;
    std::vector<std::shared_ptr<const cv::Mat>> fovea_ptrs;
    // Prepare display areas
    cv::Rect wide_view_tile;
    std::vector<cv::Rect> fovea_tiles;
    static const unsigned pad = 20;
    const unsigned num_tiles = pipe_tile_b.size(),
                   num_cols = num_tiles <= 1 ? 1 : 2,
                   num_rows = (num_tiles / num_cols) + 1;
    const unsigned w = canvas.shape().w, h = canvas.shape().h / (num_rows + 1);
    wide_view_tile =
        num_cols > 1 ? cv::Rect(0, 0, w, h) : cv::Rect(0, 0, w, h - pad);
    for (unsigned row = 0; row < num_rows; row++) {
      for (unsigned col = 0; col < num_cols; col++) {
        fovea_ptrs.push_back(nullptr);
        if (num_cols == 1) {
          fovea_tiles.push_back(cv::Rect(0, h + pad, w, h - pad));
        } else if (col == 0) {
          fovea_tiles.push_back(
              cv::Rect(0, h * (row + 1) + pad, (w / 2) - pad, h - pad));
        } else if (col == 1) {
          fovea_tiles.push_back(
              cv::Rect(w + pad, h * (row + 1) + pad, (w / 2) - pad, h - pad));
        }
      }
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
          canvas.show(*wide_ptr, wide_view_tile);
        }
        for (unsigned int i = 0; i < pipe_tile_b.size(); i++) { // Fovea streams
          auto next_ptr = pipe_tile_b[i]->read();
          if (next_ptr == nullptr)
            continue; // No Data Available
          if (next_ptr == fovea_ptrs[i])
            continue; // Same Data
          // Render to canvas
          canvas.show(*next_ptr, fovea_tiles[i]);
          // Update pointer
          fovea_ptrs[i] = next_ptr;
        }
        canvas.apply();
      }
    } catch (Threading::END &e) {
      // Normal termination
    }
    CATCH_ASSERT(;);
    // Restore splash screen
    canvas.clear().show(splash).apply();
  } catch (std::exception &e) {
    std::cerr << "[thread::display] " << e.what() << std::endl;
  }
  pipe_tile_a.close();
  for (auto &pipe : pipe_tile_b)
    pipe->close();
  std::cout << "[thread::display] terminated." << std::endl;
}

} // namespace thread