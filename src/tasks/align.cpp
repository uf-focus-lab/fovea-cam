#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>

#include "GUI.h"
#include "global.h"
#include "graphics/shared.h"
#include "tasks.h"

#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>

using namespace graphics;

struct Shift {
  int x, y;
} wide_shift = {0, 0};

void draw_markers(cv::Mat &frame, cv::Scalar color,
                  struct Shift *shift = nullptr) {
  const int w = frame.cols, h = frame.rows, s = 100, d = 160, t = 6;
  const int cx = w / 2 + (shift != nullptr ? shift->x : 0),
            cy = h / 2 + (shift != nullptr ? shift->y : 0);
  cv::drawMarker(frame, {cx - d, cy}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {cx + d, cy}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {cx, cy - d}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {cx, cy + d}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {0, cy}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {w, cy}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {cx, 0}, color, cv::MARKER_CROSS, s, t);
  cv::drawMarker(frame, {cx, h}, color, cv::MARKER_CROSS, s, t);
  cv::rectangle(frame, {cx - d, cy - d, 2 * d, 2 * d}, color, t);
}

#undef LOGNAME
#define LOGNAME "[task:align:renderer-wide] "

std::thread wide_renderer(Tile &tile, MatPipe &view_in, bool &zoom) {
  return std::thread([&]() {
    try {
      tile.auto_raster = false;
      auto frame = view_in.read();
      while (!global::flag_term) {
        view_in.next(frame, true);
        cv::Mat view;
        if (zoom) {
          const auto &s = global::config.lens.scale;
          // Crop to center of the frame
          const int w = rint(frame->cols / s), h = rint(frame->rows / s);
          cv::Rect roi = {(frame->cols - w) / 2, (frame->rows - h) / 2, w, h};
          roi.x += wide_shift.x;
          roi.y += wide_shift.y;
          tile.use((*frame)(roi));
          draw_markers(tile.mg, color::red());
        } else {
          tile.use(*frame);
          draw_markers(tile.mg, color::red(), &wide_shift);
        }
        tile.raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    view_in.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:align:renderer-fovea] "

std::thread fovea_renderer(Tile &tile, FoveaPipe &view_in) {
  return std::thread([&]() {
    try {
      tile.auto_raster = false;
      auto fovea = view_in.read();
      while (!global::flag_term) {
        view_in.next(fovea, true);
        auto view = fovea->mat.clone();
        tile.use(view);
        draw_markers(tile.mg, color::green());
        tile.raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    view_in.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:align] "

int norm(double val) {
  int k = val < 0 ? -100 : 100;
  val = std::abs(val);
  if (val < 0.1)
    return 0;
  return k * (val - 0.1) / 0.4;
}

void tasks::align(Context &ctx) {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4;
  const int content_h = h - btn_h;
  const int img_h = content_h / 2;
  // Create tiles
  int x = 0, y = 0;
  Tile wide_tile(cv::Rect{0, y, w, img_h}, pad);
  wide_tile.tbox({0, 0.9, 1, 0.1});
  wide_tile.style.text.inset = 0.3;
  wide_tile.style.text.bg = color::mono(0, 0.2);
  wide_tile.style.text.color = color::cyan();
  y += img_h;
  Tile fovea_tile(cv::Rect{0, y, w, img_h}, pad);
  fovea_tile.tbox({0, 0.9, 1, 0.1});
  fovea_tile.style.text.inset = 0.3;
  fovea_tile.style.text.bg = color::mono(0, 0.2);
  fovea_tile.style.text.color = color::yellow();
  y += img_h;
  auto back_btn = GUI::back_btn(cv::Rect{x, y, btn_w, btn_h}, pad);
  x += btn_w;
  Tile zoom_btn(cv::Rect{x, y, btn_w * 3, btn_h}, pad);
  x += btn_w;
  bool zoom = false;
  std::vector<Tile *> tiles = {
      &wide_tile.use([](Tile &tile, bool) {
        if (!tile.is_active())
          return;
        wide_shift.x = norm(tile.val.x - 0.5);
        wide_shift.y = norm(tile.val.y - 0.5);
      }),
      &fovea_tile,
      &back_btn,
      &zoom_btn.as(TileMode::BUTTON)
           .text("ZOOM IN")
           .use([&zoom](Tile &tile, bool) {
             zoom = !zoom;
             tile.text(zoom ? "ZOOM OUT" : "ZOOM IN ");
           }),
  };
  // Launch worker threads
  std::vector<global::ThreadInfo> threads;
  threads.push_back({
      "renderer/wide",
      wide_renderer(wide_tile, ctx.cap_wide, zoom),
  });
  threads.push_back({
      "renderer/fovea",
      fovea_renderer(fovea_tile, ctx.cap_fovea),
  });
  // Render loop
  try {
    ctx.mems_pos.flush().write({0.0, 0.0});
    while (!global::flag_term) {
      auto pos = fb.wait_pointer(false);
      for (auto &el : tiles)
        el->handle(pos);
      canvas.show(tiles).apply(fb, &pos);
    }
  }
  EXPECT_END_OF_STREAM
  CATCH_ASSERT(LOGNAME)
  ctx.close();
  for (auto &el : threads) {
    std::cerr << LOGNAME "waiting for " << el.name << std::endl;
    el.thread.join();
  }
  std::cerr << LOGNAME "terminated." << std::endl;
}
