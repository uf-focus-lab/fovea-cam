#include "threads.h"

#include "graphics/canvas.h"
#include "graphics/x11.h"
#include "util/assert.h"

#include "splash.png.h"

namespace thread {

void display(Threading::FastIO<cv::Mat> &pipe_tile_a,
             std::vector<Threading::FastIO<cv::Mat> *> pipe_tile_b) {
  try {
    graphics::X11FB fb;
    graphics::Canvas canvas(fb.shape().w, fb.shape().h);
    const cv::Mat splash(SPLASH_PNG_H, SPLASH_PNG_W, CV_8UC4,
                   (char *)SPLASH_PNG_DATA);
    canvas.clear().show(splash).apply(fb.buffer());
    fb.sync();
    std::this_thread::sleep_for(std::chrono::seconds(5));
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
                   num_rows = (num_tiles / num_cols) + num_tiles % num_cols;
    const unsigned w = canvas.shape().w, h = canvas.shape().h / (num_rows + 1);
    wide_view_tile =
        num_cols > 1 ? cv::Rect(0, 0, w, h) : cv::Rect(0, 0, w, h - pad);
    std::cerr << "[thread::display] " << num_cols << "x" << num_rows
              << " fovea tiles." << std::endl;
    for (unsigned row = 0; row < num_rows; row++) {
      for (unsigned col = 0; col < num_cols; col++) {
        fovea_ptrs.push_back(nullptr);
        if (num_cols == 1) {
          fovea_tiles.push_back(cv::Rect(0, h + pad, w, h - pad));
        } else if (col == 0) {
          fovea_tiles.push_back(
              cv::Rect(0, h * (row + 1) + pad, (w / 2) - pad, h - pad));
        } else if (col == 1) {
          fovea_tiles.push_back(cv::Rect(w / 2 + pad, h * (row + 1) + pad,
                                         (w / 2) - pad, h - pad));
        }
      }
    }
    try {
      bool flag_new;
      while (1) {
        flag_new = false;
        { // Wide angle
          auto next_ptr = pipe_tile_a.read();
          if (next_ptr == nullptr)
            continue; // No Data Available
          if (next_ptr == wide_ptr)
            continue; // Same Data
          flag_new = true;
          // Render to canvas
          canvas.show(*next_ptr, wide_view_tile);
          // Update pointer and render to canvas
          wide_ptr = next_ptr;
        }
        for (unsigned int i = 0; i < pipe_tile_b.size(); i++) { // Fovea streams
          auto next_ptr = pipe_tile_b[i]->read();
          if (next_ptr == nullptr)
            continue; // No Data Available
          if (next_ptr == fovea_ptrs[i])
            continue; // Same Data
          flag_new = true;
          // Render to canvas
          canvas.show(*next_ptr, fovea_tiles[i]);
          // Update pointer
          fovea_ptrs[i] = next_ptr;
        }
        if (flag_new) {
          canvas.apply(fb.buffer());
          fb.sync();
        }
      }
    } catch (Threading::END &e) {
      // Normal termination
    }
    CATCH_ASSERT(;)
    catch (std::exception &e) {
      std::cerr << "[thread::display] " << e.what() << std::endl;
    }
    catch (...) {
      std::cerr << "[thread::display] Unknown exception." << std::endl;
    };
    // Restore splash screen
    canvas.clear().show(splash).apply(fb.buffer());
    fb.sync();
  } catch (std::exception &e) {
    std::cerr << "[thread::display] " << e.what() << std::endl;
  }
  pipe_tile_a.close();
  for (auto &pipe : pipe_tile_b)
    pipe->close();
  std::cerr << "[thread::display] terminated." << std::endl;
}

} // namespace thread