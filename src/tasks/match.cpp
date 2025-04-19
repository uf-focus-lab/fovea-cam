#include <iostream>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <thread>

#include "GUI.h"
#include "global.h"
#include "tasks.h"

#include <calib/calib.h>
#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>
#include <util/clamp.h>
#include <util/fmt.h>

using namespace graphics;

#undef LOGNAME
#define LOGNAME "[task:match:matcher] "

std::thread matcher(Context &ctx, Tile &tile,
                    threading::FastIO<cv::Rect> &roi_out) {
  return std::thread([&]() {
    try {
      tile.auto_raster = false;
      auto fovea = ctx.cap_fovea.read();
      while (!global::flag_term) {
        ctx.cap_fovea.next(fovea, true);
        // std::cerr << LOGNAME "FOVEA: (" << fmt(fovea->x, 2, 2) << ", "
        //           << fmt(fovea->y, 2, 2) << ") @" << fovea->tag << std::endl;
        // Convert volt range from [-90, 90] to [0, 1]
        cv::Point2d volt = fovea->volt();
        auto pos = calib::cvt(calib::VtoP, volt) + calib::shift;
        // Get latest wide frame
        auto wide = ctx.cap_wide.read();
        if (wide == nullptr)
          continue;
        // Calculate cropping region
        cv::Rect roi = calib::roi(pos, wide->size(), global::config.lens.scale);
        // Push to pipe
        try {
          roi_out.write(roi);
          tile.use(calib::view(*wide, roi)).raster();
        } catch (cv::Exception &e) {
          std::cerr << LOGNAME "out of bound roi: " << roi << std::endl;
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    roi_out.close();
    ctx.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:match:calibrator] "

enum {
  // Calibration mode
  STATE_INIT,
  STATE_BUSY,
  STATE_DONE,
} calibrator_state = STATE_INIT;

void calibrator(Context &ctx) {
  std::thread([&]() {
    try {
      auto wide = ctx.cap_wide.read();
      auto fovea = ctx.cap_fovea.read();
      if (wide == nullptr || fovea == nullptr) {
        calibrator_state = STATE_INIT;
        std::cerr << LOGNAME "image not available" << std::endl;
        return;
      }
      const double scale = 1.0 / global::config.lens.scale;
      cv::Point2d volt = fovea->volt();
      auto pos = calib::cvt(calib::VtoP, volt) + calib::shift;
      cv::Mat canvas, kernel;
      // Step 1: scale down the fovea to match dpi with wide
      cv::cvtColor(fovea->mat, kernel, cv::COLOR_BGRA2GRAY);
      cv::resize(kernel, kernel, {}, scale, scale, cv::INTER_AREA);
      const int w = kernel.cols, h = kernel.rows;
      // Step 2: Crop the canvas to +- 50% range of the initial guess
      cv::cvtColor(*wide, canvas, cv::COLOR_BGRA2GRAY);
      cv::copyMakeBorder(canvas, canvas, h, h, w, w, cv::BORDER_REPLICATE);
      cv::Point center = {
          static_cast<int>(pos.x * static_cast<double>(wide->cols)),
          static_cast<int>(pos.y * static_cast<double>(wide->rows))};
      cv::Rect roi = {center.x, center.y, 2 * w, 2 * h};
      canvas = calib::view(canvas, roi).clone();
      // Step 4: Find the best match
      cv::Mat result;
      cv::matchTemplate(canvas, kernel, result, cv::TM_CCOEFF_NORMED);
      // Step 5: Calculate the offset
      cv::Point offset;
      cv::minMaxLoc(result, nullptr, nullptr, nullptr, &offset);
      offset.x -= w / 2;
      offset.y -= h / 2;
      std::cerr << LOGNAME "offset: " << offset << std::endl;
      // Finally, write result to calib::shift, and reset flag
      calib::shift.x +=
          static_cast<double>(offset.x) / static_cast<double>(wide->cols);
      calib::shift.y +=
          static_cast<double>(offset.y) / static_cast<double>(wide->rows);
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    calibrator_state = STATE_DONE;
  }).detach();
}

#undef LOGNAME
#define LOGNAME "[task:match] "

void tasks::match(Context &ctx) {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4;
  const int note_h = btn_h / 2;
  const int content_h = h - btn_h - note_h;
  const int img_h1 = std::min(content_h / 2, 2 * w / 5);
  const int img_h2 = content_h - img_h1;
  // Create tiles
  int y = 0;
  Tile match_tile(cv::Rect{0, y, w / 2, img_h1}, pad);
  match_tile.use([](Tile &tile, bool state_change) {
    static cv::Point2d val;
    if (state_change) {
      if (tile.is_active())
        val = tile.val;
      else
        tile.text("");
    } else if (tile.is_active()) {
      auto delta = tile.val - val;
      val = tile.val;
      global::config.lens.scale += delta.x - delta.y;
      std::stringstream ss;
      ss << "SCALE " << fmt(global::config.lens.scale, 2, 2, ' ', false) << "x";
      tile.text(ss.str());
    }
  });
  Tile fovea_tile(cv::Rect{w / 2, y, w / 2, img_h1}, pad);
  y += img_h1;
  Tile wide_tile(cv::Rect{0, y, w, img_h2}, pad);
  y += img_h2;
  Tile notes(cv::Rect{0, y, w, note_h});
  notes.tbox({0, .25, 1, .5});
  notes.style.bg = color::mono(0);
  y += note_h;
  auto back_btn = GUI::back_btn(cv::Rect{0, y, btn_w, btn_h}, pad);
  Tile reset_btn(cv::Rect{btn_w, y, btn_w, btn_h}, pad);
  Tile calib_btn(cv::Rect{btn_w * 2, y, btn_w * 2, btn_h}, pad);
  std::vector<Tile *> tiles = {
      &match_tile,
      &fovea_tile,
      &wide_tile
           .use(calib::cvt(calib::VtoP, {0.5, 0.5}) + calib::shift) //
           .use([&ctx, &notes](Tile &tile, bool) {
             auto pos = calib::cvt(calib::PtoV, tile.val - calib::shift);
             pos.x = clamp(pos.x, 0.0, 1.0);
             pos.y = clamp(pos.y, 0.0, 1.0);
             double Vx = pos.x * 180.0 - 90.0, Vy = pos.y * 180.0 - 90.0;
             static uint8_t tag = 0;
             ctx.mems_pos.flush().write({Vx, Vy, tag++});
             // Update notes
             std::stringstream ss;
             ss << "V " << fmt(Vx, 2, 2) << ", " << fmt(Vy, 2, 2) << " | "
                << "Z " << fmt(global::config.lens.scale, 2, 2, ' ', false)
                << "x"
                << " | "
                << "S " << fmt(calib::shift.x, 1, 3) + '%' << ", "
                << fmt(calib::shift.y, 1, 3) + '%';
             notes.text(ss.str());
           }),
      &notes.text("current task: match"),
      &back_btn,
      &reset_btn.as(TileMode::BUTTON)
           .text("RESET")
           .use([&wide_tile](Tile &tile, bool) {
             calib::shift = {0, 0};
             wide_tile.render(true);
           }),
      &calib_btn.as(TileMode::BUTTON)
           .text("CALIBRATE")
           .use([&ctx](Tile &tile, bool) {
             if (calibrator_state == STATE_INIT) {
               calibrator_state = STATE_BUSY;
               tile.text("CALIBRATING...");
               calibrator(ctx);
             }
           }),
  };
  // Launch worker threads
  threading::FastIO<cv::Rect> roi_pipe;
  std::vector<global::ThreadInfo> threads;
  threads.push_back({
      "matcher",
      matcher(ctx, match_tile, roi_pipe),
  });
  threads.push_back({
      "mat-renderer/wide",
      GUI::mat_renderer(ctx.cap_wide, wide_tile, &roi_pipe),
  });
  threads.push_back({
      "mat-renderer/fovea",
      GUI::fovea_renderer(ctx.cap_fovea, fovea_tile, -1),
  });
  // Render loop
  try {
    while (!global::flag_term) {
      auto pos = fb.wait_pointer();
      for (auto &el : tiles)
        el->handle(pos);
      if (calibrator_state == STATE_DONE) {
        calibrator_state = STATE_INIT;
        calib_btn.text("CALIBRATE");
        wide_tile.render(true);
      }
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
