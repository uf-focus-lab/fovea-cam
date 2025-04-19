#include <glob.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "global.h"

#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/clamp.h>

#include "splash.png.h"
#include "uf.logo.png.h"

#undef LOG_NAME
#define LOG_NAME "[kiosk] "

using namespace graphics;

int run_task(const std::string task);

int kiosk() {
  std::cerr << LOG_NAME "Entering Kiosk Mode" << std::endl;
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  // Prepare tiles for interaction
  const int w = fb.shape().width, pad = w / 64;
  const int btn_h = w / 8, btn_w4 = w / 4, btn_w3 = w / 3;
  int x, y = fb.shape().height - 3 * btn_h;
  // Tile for splash image
  const cv::Mat splash_mat(SPLASH_PNG_H, SPLASH_PNG_W, CV_8UC4,
                           (char *)SPLASH_PNG_DATA);
  Tile splash({0, 0, w, y}, 2 * pad);
  splash.style.bg = color::mono(0);
  // ======================== Calibration Tasks ========================
  y += btn_h;
  x = 0;
  Tile btn_tune(cv::Rect{x, y, btn_w4, btn_h}, pad, TileMode::BUTTON);
  x += btn_w4;
  Tile btn_align(cv::Rect{x, y, btn_w4, btn_h}, pad, TileMode::BUTTON);
  x += btn_w4;
  Tile btn_checker(cv::Rect{x, y, btn_w4, btn_h}, pad, TileMode::BUTTON);
  x += btn_w4;
  Tile btn_calib(cv::Rect{x, y, btn_w4, btn_h}, pad, TileMode::BUTTON);
  // ======================== Application Tasks ========================
  y += btn_h;
  x = 0;
  Tile btn_match(cv::Rect{x, y, btn_w3, btn_h}, pad, TileMode::BUTTON);
  x += btn_w3;
  Tile btn_track(cv::Rect{x, y, btn_w3, btn_h}, pad, TileMode::BUTTON);
  x += btn_w3;
  Tile btn_stabilize(cv::Rect{x, y, btn_w3, btn_h}, pad, TileMode::BUTTON);
  // Enter event loop
  std::string task = "";
  std::vector<Tile *> tiles = {
      &btn_tune
           .text("Tune") //
           .use([&task](Tile &, bool) { task = "tune"; }),
      &btn_align
           .text("Align") //
           .use([&task](Tile &, bool) { task = "align"; }),
      &btn_checker
           .text("Checker") //
           .use([&task](Tile &, bool) { task = "checker"; }),
      &btn_calib
           .text("ArUco") //
           .use([&task](Tile &, bool) { task = "aruco"; }),
      &btn_match
           .text("Match") //
           .use([&task](Tile &, bool) { task = "match"; }),
      &btn_track
           .text("Track") //
           .use([&task](Tile &, bool) { task = "track"; }),
      &btn_stabilize
           .text("Stabilize") //
           .use([&task](Tile &, bool) { task = "stabilize"; }),
  };
  canvas.clear().show(splash.use(splash_mat)).show(tiles).apply(fb);

  std::cerr << LOG_NAME "Start interaction" << std::endl;
  while (!global::flag_term) {
    const auto pos = fb.wait_pointer();
    // Check for corresponding tile
    for (auto tile : tiles)
      tile->handle(pos);
    canvas.show(tiles).apply(fb, &pos);
    if (task != "") {
      run_task(task);
      task = "";
    }
  }
  return 0;
}

void splash() {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  const int w = fb.shape().width, h = fb.shape().height;
  const cv::Mat splash_mat(SPLASH_PNG_H, SPLASH_PNG_W, CV_8UC4,
                           (char *)SPLASH_PNG_DATA);
  const cv::Mat logo_mat(UF_LOGO_PNG_H, UF_LOGO_PNG_W, CV_8UC4,
                         (char *)UF_LOGO_PNG_DATA);
  const int logo_h = std::min(h / 8, w / 8), pad = w / 64;
  Tile splash({0, 0, w, h - 3 * logo_h}, 2 * pad);
  Tile logo({0, h - 2 * logo_h, w, logo_h}, 2 * pad);
  splash.style.bg = logo.style.bg = color::mono(0);
  canvas.clear()
      .show(splash.use(splash_mat))
      .show(logo.use(logo_mat))
      .apply(fb);
}
