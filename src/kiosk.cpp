#include <cmath>
#include <glob.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "global.h"

#include "graphics/canvas.h"
#include "graphics/tile.h"
#include "util/clamp.h"

#include "splash.png.h"

#undef LOG_NAME
#define LOG_NAME "[kiosk] "

using namespace graphics;

std::string str(std::string prefix, double val, int precision = 2) {
  std::stringstream stream;
  stream << prefix << std::fixed << std::setprecision(precision) << val;
  return stream.str();
}

std::string str(double val, int precision = 2) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << val;
  return stream.str();
}

std::string EXP(cv::Point2d val) {
  auto &exp = global::config.exp;
  exp = clamp<double>(val.x, 0, 1) * 100.0;
  return "EXP = " + str(exp);
}

std::string FPS(cv::Point2d val) {
  auto &fps = global::config.fps;
  if (val.x > 0.1)
    fps = clamp<double>(val.x, 0, 1) * 110 - 10.5;
  else
    fps = -1.0;
  return "FPS = " + (fps > 0.0 ? str(fps) : "N/A");
}

std::string GAIN(cv::Point2d val) {
  auto &gain = global::config.gain;
  if (val.x > 0.05)
    gain = clamp<double>(val.x, 0, 1) * 40.0;
  else
    gain = 0.0;
  return "GAIN = " + str(gain);
}

int run_task(const std::string task);

cv::Rect pad_rect(int x, int y, int w, int h, int pad) {
  return cv::Rect(x + pad, y + pad, w - pad * 2, h - pad * 2);
}

int kiosk() {
  std::cerr << LOG_NAME "Entering Kiosk mode" << std::endl;
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height / 2 / 4,
            pad = rint(double(h) / 8.f);
  const cv::Scalar bg(16, 16, 16, 255);
  const cv::Scalar gr(96, 80, 64, 255);
  const cv::Scalar fg(104, 221, 237, 255);
  int y = fb.shape().height / 2;
  // Tile for splash image
  const cv::Mat splash(SPLASH_PNG_H, SPLASH_PNG_W, CV_8UC4,
                       (char *)SPLASH_PNG_DATA);
  canvas.clear().show(splash, pad_rect(0, 0, w, y, pad));
  // Tile for FPS slider
  Tile fps(cv::Rect{0, y, w, h}, pad);
  // Tile for EXP slider
  y += h;
  Tile exp(cv::Rect{0, y, w, h}, pad);
  exp.val.x = global::config.exp / 100.0;
  // Tile for EXP slider
  y += h;
  Tile gain(cv::Rect{0, y, w, h}, pad);
  gain.val.x = global::config.gain / 40.0;
  // Tile for action buttons
  y += h;
  Tile btn_tune(cv::Rect{0, y, w / 4, h}, pad),
      btn_track(cv::Rect{w / 4, y, w / 4, h}, pad),
      btn_match(cv::Rect{2 * w / 4, y, w / 4, h}, pad),
      btn_rec(cv::Rect{3 * w / 4, y, w / 4, h}, pad);

  std::vector<Tile *> tiles = {
      &fps.fill(bg).fill(gr, fps.val.x).text(FPS, fg),
      &exp.fill(bg).fill(gr, exp.val.x).text(EXP, fg),
      &gain.fill(bg).fill(gr, gain.val.x).text(GAIN, fg),
      &btn_tune.fill(bg).text("TUNE", fg),
      &btn_track.fill(bg).text("TRACK", fg),
      &btn_match.fill(bg).text("MATCH", fg),
      &btn_rec.fill(bg).text("CAPTURE", fg),
  };
  canvas.show(tiles).apply(fb);

  std::cerr << LOG_NAME "Start interaction" << std::endl;
  // Enter event loop
  std::string task = "";
  while (1) {
    if (task != "") {
      run_task(task);
      task = "";
    }
    const auto pos = fb.wait_pointer(true);
    // Check for corresponding tile
    if (exp.handle(pos)) {
      exp.fill(bg).fill(gr, exp.val.x);
    }
    if (fps.handle(pos)) {
      fps.fill(bg).fill(gr, fps.val.x);
    }
    if (gain.handle(pos)) {
      gain.fill(bg).fill(gr, gain.val.x);
    }
    if (btn_tune.is_active()) {
      if (!btn_tune.handle(pos)) {
        btn_tune.fill(bg);
        task = "move";
      }
    } else if (btn_tune.handle(pos)) {
      btn_tune.fill(gr);
    }

    if (btn_track.is_active()) {
      if (!btn_track.handle(pos)) {
        btn_track.fill(bg);
        task = "track";
      }
    } else if (btn_track.handle(pos)) {
      btn_track.fill(gr);
    }

    if (btn_match.is_active()) {
      if (!btn_match.handle(pos)) {
        btn_match.fill(bg);
        task = "match";
      }
    } else if (btn_match.handle(pos)) {
      btn_match.fill(gr);
    }

    if (btn_rec.is_active()) {
      if (!btn_rec.handle(pos)) {
        btn_rec.fill(bg);
        task = "capture";
      }
    } else if (btn_rec.handle(pos)) {
      btn_rec.fill(gr);
    }

    canvas.show(tiles).apply(fb, &pos);
  }
  return 0;
}
