#include <iostream>

#include "global.h"
#include "tasks.h"

#include "calib/calib.h"
#include "graphics/canvas.h"
#include "graphics/tile.h"
#include "util/assert.h"
#include "util/clamp.h"

using namespace graphics;

std::thread matcher(Context &ctx, MatPipe &match_out) {
  return std::thread([&]() {
    try {
      auto fovea = ctx.cap_fovea.read();
      while (!global::flag_term) {
        auto _fovea = ctx.cap_fovea.read();
        if (_fovea == fovea || _fovea == nullptr)
          continue;
        fovea = _fovea;
        // Get latest image
        auto wide = ctx.cap_wide.read();
        if (wide == nullptr)
          continue;
        // Convert volt range from [-90, 90] to [0, 1]
        double x = fovea->x / 180.0 + 0.5, y = fovea->y / 180.0 + 0.5;
        auto pos = calib::cvt(calib::P, x, y);
        // Calculate cropping region
        cv::Rect roi = calib::roi(pos, wide->size(), global::config.zoom);
        // Push to pipe
        try {
          match_out.write((*wide)(roi).clone());
        } catch (cv::Exception &e) {
          std::cerr << "[match] out of bound roi: " << roi << std::endl;
        }
      }
    } catch (Threading::END &) {
      // Normal termination
    }
    CATCH_ASSERT("[task::match::matcher]");
    match_out.close();
    ctx.close();
    std::cerr << "[task::match] terminated." << std::endl;
  });
}

std::thread display(Context &ctx, MatPipe &match_in) {
  return std::thread([&]() {
    auto &fb = *global::fb;
    Canvas canvas(fb.shape());
    canvas.clear();
    // Theme colors
    const cv::Scalar bg(32, 32, 32, 255);
    const cv::Scalar gr(64, 64, 64, 255);
    const cv::Scalar fg(192, 192, 64, 255);
    // Prepare tiles for interaction
    const int w = fb.shape().width, h = fb.shape().height;
    const int pad = w / 128;
    const int btn_h = w / 16, btn_w = w / 4;
    const int img_h = (h - btn_h) / 2;
    // Create tiles
    Tile exit_btn(cv::Rect{0, 0, btn_w, btn_h}, pad);
    Tile wide_tile(cv::Rect{0, btn_h, w, img_h}, pad);
    Tile match_tile(cv::Rect{0, btn_h + img_h, w / 2, img_h}, pad);
    Tile fovea_tile(cv::Rect{w / 2, btn_h + img_h, w / 2, img_h}, pad);
    std::vector<Tile *> tiles = {
        &exit_btn.fill(bg).text("EXIT", fg),
        &wide_tile.fill(bg),
        &match_tile.fill(bg),
        &fovea_tile.fill(bg),
    };
    // Render loop
    try {
      auto wide = ctx.cap_wide.read();
      auto fovea = ctx.cap_fovea.read();
      auto match = match_in.read();
      while (!global::flag_term) {
        auto pos = fb.wait_pointer(false);

        auto _wide = ctx.cap_wide.read();
        if (_wide != wide && _wide != nullptr) {
          wide = _wide;
          wide_tile.fill(*wide);
        }

        auto _fovea = ctx.cap_fovea.read();
        if (_fovea != fovea && _fovea != nullptr) {
          fovea = _fovea;
          fovea_tile.fill(fovea->mat);
        }

        auto _match = match_in.read();
        if (_match != match && _match != nullptr) {
          match = _match;
          match_tile.fill(*match);
        }

        if (wide_tile.handle(pos)) {
          double x = wide_tile.val.x, y = wide_tile.val.y;
          auto pos = calib::cvt(calib::V, x, y);
          pos.x = clamp(pos.x, 0.0, 1.0);
          pos.y = clamp(pos.y, 0.0, 1.0);
          ctx.mems_pos.write({pos.x * 180.0 - 90.0, pos.y * 180.0 - 90.0, 0});
          // Draw box
          auto pos_px = calib::cvt(calib::P, pos.x, pos.y);
          auto roi = calib::roi(pos_px, wide->size(), global::config.zoom);
          wide_tile.fg = color::black(0);
          cv::rectangle(wide_tile.fg, roi, color::red(255), 4);
        }

        if (exit_btn.handle(pos)) {
          exit_btn.fill(gr);
          global::flag_term = true;
        }

        canvas.show(tiles).apply(fb);
      }
    } catch (Threading::END &) {
      // Normal termination
    }
    CATCH_ASSERT("[task::match::display]")
    match_in.close();
    ctx.close();
    std::cerr << "[task::match::display] terminated." << std::endl;
  });
}

void tasks::match(Context &ctx) {
  MatPipe match;
  ctx.threads.push_back({
      "task/match/matcher",
      matcher(ctx, match),
  });
  ctx.threads.push_back({
      "task/match/display",
      display(ctx, match),
  });
}
