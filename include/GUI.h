#pragma once

#include "global.h"

#include <graphics/tile.h>

namespace GUI {

template <typename T> static inline T mul(T a, double b) {
  return static_cast<T>(static_cast<double>(a) * b);
}

extern graphics::Tile &back_btn(cv::Rect box, int pad = 0);

std::thread mat_renderer(MatPipe &mat_pipe, graphics::Tile &tile,
                         threading::FastIO<cv::Rect> *const roi_pipe = nullptr);

std::thread fovea_renderer(FoveaPipe &fovea_pipe, graphics::Tile &tile,
                           const int tag = -1);

} // namespace GUI
