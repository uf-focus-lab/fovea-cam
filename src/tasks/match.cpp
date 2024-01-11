#include <iostream>

#include "global.h"
#include "tasks.h"

#include "calib/calib.h"
#include "graphics/canvas.h"
#include "graphics/tile.h"
#include "threading/fast_io.h"
#include "util/assert.h"
#include "util/clamp.h"

#include <sstream>

using namespace graphics;

#undef LOGNAME
#define LOGNAME "[task:match:matcher] "

void matcher(Context &ctx, MatPipe &match_out,
             threading::FastIO<cv::Rect> &roi_out) {
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
      auto pos = calib::cvt(calib::VtoP, x, y);
      // Calculate cropping region
      cv::Rect roi = calib::roi(pos, wide->size(), global::config.zoom);
      // Push to pipe
      try {
        match_out.write((*wide)(roi).clone());
        roi_out.write(roi);
      } catch (cv::Exception &e) {
        std::cerr << LOGNAME "out of bound roi: " << roi << std::endl;
      }
    }
  } catch (threading::END &) {
    // Normal termination
  }
  CATCH_ASSERT(LOGNAME);
  match_out.close();
  roi_out.close();
  ctx.close();
  std::cerr << LOGNAME "terminated." << std::endl;
}

#undef LOGNAME
#define LOGNAME "[task:match] "

void tasks::match(Context &ctx) {
  MatPipe match_pipe;
  threading::FastIO<cv::Rect> roi_pipe;
  auto match_thread = std::thread(matcher, std::ref(ctx), std::ref(match_pipe),
                                  std::ref(roi_pipe));
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Theme colors
  const cv::Scalar bg(16, 16, 16, 255);
  const cv::Scalar gr(96, 80, 64, 255);
  const cv::Scalar fg(104, 221, 237, 255);
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4;
  const int img_h = (h - 2 * btn_h) / 2;
  // Create tiles
  int y = 0;
  Tile match_tile(cv::Rect{0, y, w / 2, img_h}, pad);
  Tile fovea_tile(cv::Rect{w / 2, y, w / 2, img_h}, pad);
  y += img_h;
  Tile wide_tile(cv::Rect{0, y, w, img_h}, pad);
  y += img_h;
  Tile notes(cv::Rect{0, y, w, btn_h}, pad);
  y += btn_h;
  Tile exit_btn(cv::Rect{0, y, btn_w, btn_h}, pad);
  Tile reset_btn(cv::Rect{btn_w, y, btn_w, btn_h}, pad);
  Tile calib_btn(cv::Rect{btn_w * 2, y, btn_w * 2, btn_h}, pad);
  std::vector<Tile *> tiles = {
      &match_tile.fill(bg),
      &fovea_tile.fill(bg),
      &wide_tile.fill(bg),
      &notes.text("current task: match", fg),
      &exit_btn.fill(color::red(64)).text("EXIT", fg),
      &reset_btn.fill(color::red(64)).text("RESET", fg),
      &calib_btn.fill(color::red(64)).text("CALIBRATE", fg),
  };
  // Render loop
  try {
    auto match = match_pipe.read();
    auto fovea = ctx.cap_fovea.read();
    auto wide = ctx.cap_wide.read();
    while (!global::flag_term) {
      auto pos = fb.wait_pointer(false);

      auto _match = match_pipe.read();
      if (_match != match && _match != nullptr) {
        match = _match;
        match_tile.fill(*match);
      }

      auto _fovea = ctx.cap_fovea.read();
      if (_fovea != fovea && _fovea != nullptr) {
        fovea = _fovea;
        fovea_tile.fill(fovea->mat);
      }

      auto _wide = ctx.cap_wide.read();
      if (_wide != wide && _wide != nullptr) {
        wide = _wide;
        // Draw ROI box
        auto roi = roi_pipe.read();
        if (roi != nullptr) {
          cv::Mat mat = wide->clone();
          cv::rectangle(mat, *roi, color::red(128), 4);
          wide_tile.fill(mat);
        } else {
          wide_tile.fill(*wide);
        }
      }

      if (wide_tile.handle(pos)) {
        double x = wide_tile.val.x, y = wide_tile.val.y;
        auto pos = calib::cvt(calib::PtoV, x, y);
        pos.x = clamp(pos.x, 0.0, 1.0);
        pos.y = clamp(pos.y, 0.0, 1.0);
        double Vx = pos.x * 180.0 - 90.0, Vy = pos.y * 180.0 - 90.0;
        ctx.mems_pos.write({Vx, Vy, 0});
        // Update notes
        std::stringstream ss;
        // ss << "Vx: " << std::fixed << std::setprecision(2) << Vx << ", "
        //    << "Vy: " << std::fixed << std::setprecision(2) << Vy;
        ss << "src: " << wide_tile.val << ", "
           << "dst: " << pos;
        notes.text(ss.str(), fg);
      }

      if (exit_btn.button(pos, bg, gr))
        global::flag_term = true;

      if (reset_btn.button(pos, bg, gr))
        ctx.mems_pos.write({0, 0, 0});

      canvas.show(tiles).apply(fb, &pos);
    }
  } catch (threading::END &) {
    // Normal termination
  }
  CATCH_ASSERT(LOGNAME)
  match_pipe.close();
  ctx.close();
  std::cerr << LOGNAME "waiting for matcher." << std::endl;
  match_thread.join();
  std::cerr << LOGNAME "terminated." << std::endl;
}
