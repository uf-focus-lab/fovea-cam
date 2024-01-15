#include "GUI.h"

#include "util/assert.h"

#undef LOGNAME
#define LOGNAME "[GUI:mat-renderer] "

std::thread GUI::mat_renderer(MatPipe &mat_pipe, graphics::Tile &tile,
                              threading::FastIO<cv::Rect> *const roi_pipe) {
  return std::thread([&, roi_pipe]() {
    try {
      tile.auto_raster = false;
      auto frame = mat_pipe.read();
      auto roi = roi_pipe != nullptr ? roi_pipe->read() : nullptr;
      cv::Mat roi_mat;
      bool flag_rect = false;
      while (!global::flag_term) {
        if (mat_pipe.next(frame)) {
          roi_mat = frame->clone();
          flag_rect = false;
        }
        if (frame == nullptr)
          continue;
        if (roi_pipe == nullptr) {
          tile.use(roi_mat).raster();
          continue;
        }
        if (roi_pipe->next(roi) || !flag_rect) {
          if (roi == nullptr)
            continue;
          if (!flag_rect)
            roi_mat = frame->clone();
          cv::rectangle(roi_mat, *roi, color::red(), 4);
          tile.use(roi_mat).raster();
          flag_rect = true;
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    mat_pipe.close();
    if (roi_pipe != nullptr)
      roi_pipe->close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[GUI:FoveaRenderer] "

std::thread GUI::fovea_renderer(FoveaPipe &fovea_pipe, graphics::Tile &tile,
                                const int tag) {
  return std::thread([&, tag]() {
    try {
      tile.auto_raster = false;
      auto fovea = fovea_pipe.read();
      while (!global::flag_term) {
        fovea_pipe.next(fovea, true);
        ASSERT(fovea != nullptr, LOGNAME "NOT EXPECTED: next() == nullptr");
        if (tag >= 0 && fovea->tag != tag)
          continue;
        tile.use(fovea->mat).raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    fovea_pipe.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}
