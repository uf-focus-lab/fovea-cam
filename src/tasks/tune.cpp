#include <iostream>

#include "GUI.h"
#include "global.h"
#include "tasks.h"

#include <calib/calib.h>
#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>
#include <util/clamp.h>

#include <cmath>
#include <opencv2/opencv.hpp>
#include <sstream>

using namespace graphics;

// Flag to toggle zebra patterns on over and under exposure areas
bool flag_highlight = false;

#undef LOGNAME
#define LOGNAME "[task:tune:render-helper] "

cv::Mat highlight(const cv::Mat &frame) {
  cv::Mat gray;
  cv::cvtColor(frame, gray, cv::COLOR_BGRA2GRAY);
  auto under = cv::Mat(gray < 8);
  auto over = cv::Mat(gray > 248);
  std::vector<cv::Mat> channels;
  channels.push_back(gray);
  channels.push_back(gray);
  channels.push_back(gray);
  channels.push_back(cv::Mat(gray.rows, gray.cols, CV_8UC1, {255}));
  cv::Mat result;
  cv::merge(channels, result);
  cv::copyTo(cv::Mat(result.rows, result.cols, CV_8UC4, color::blue()), result,
             under);
  cv::copyTo(cv::Mat(result.rows, result.cols, CV_8UC4, color::red()), result,
             over);
  return result;
}

std::thread render_helper(MatPipe &in, MatPipe &out) {
  return std::thread([&]() {
    try {
      auto frame = in.read();
      while (!global::flag_term) {
        if (!in.next(frame, true))
          continue;
        out.write(flag_highlight ? highlight(*frame) : *frame);
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    in.close();
    out.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

std::thread render_helper(FoveaPipe &in, MatPipe &out) {
  return std::thread([&]() {
    try {
      auto frame = in.read();
      while (!global::flag_term) {
        if (!in.next(frame, true))
          continue;
        out.write(flag_highlight ? highlight(frame->mat) : frame->mat);
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    in.close();
    out.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

static global::CamConfig *config = &global::config.wide;

std::string str(double val, int precision = 2) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << val;
  return stream.str();
}

// [0.1, 1.0] <-- linear -> [1.0, 120.0]
std::string FPS(double x) {
  auto &fps = config->fps;
  if (x > 0.1)
    fps = 1.0 + (clamp(x, 0.1, 1.0) - 0.1) * (120.0 - 1.0);
  else
    fps = -1.0;
  return "FPS = " + (fps > 0.0 ? str(fps) : "N/A");
}

cv::Point2d fps() {
  auto &fps = config->fps;
  if (fps > 1.0)
    return {(fps - 1.0) / (120.0 - 1.0) + 0.1};
  else
    return {0.0};
}

// [0.0, 1.0] <-- exp -> [1.0, 1024.0]
std::string EXP(double x) {
  auto &exp = config->exp;
  exp = std::exp2(clamp(x, 0.0, 1.0) * 10.0);
  return "EXP = " + str(exp);
}

cv::Point2d exp() {
  auto &exp = config->exp;
  return {std::log2(clamp(exp, 1.0, 1024.0)) / 10.0};
}

std::string GAIN(double x) {
  auto &gain = config->gain;
  if (x > 0.05)
    gain = clamp(x, 0.0, 1.0) * 40.0;
  else
    gain = 0.0;
  return "GAIN = " + str(gain);
}

cv::Point2d gain() {
  auto &gain = config->gain;
  return {clamp(gain, 0.0, 40.0) / 40.0};
}

// [0.0, 0.5, 1.0] <-- 2 seg linear -> [0.25, 1.0, 4.0]
std::string GAMMA(double x) {
  auto &gamma = config->gamma;
  x = clamp(x, 0.0, 1.0);
  if (x > 0.51)
    gamma = 1.0 + (x - 0.5) * 3.0;
  else if (x < 0.49)
    gamma = 0.25 + 0.75 * x / 0.5;
  else
    return "GAMMA = OFF";
  return "GAMMA = " + str(gamma);
}

cv::Point2d gamma() {
  auto &gamma = config->gamma;
  gamma = clamp(gamma, 0.25, 4.0);
  if (gamma > 1.0)
    return {0.5 + (gamma - 1.0) / 3.0};
  else if (gamma < 1.0)
    return {0.5 * (gamma - 0.25) / 0.75};
  else
    return {0.5};
}

// [0.0, 1.0] <-- linear -> [-10.0, 10.0], clipped at -5
std::string BLACK(double &x) {
  x = clamp(x, 0.25, 1.0);
  auto &black = config->black;
  if (std::abs(x - 0.5) <= 0.01) {
    x = 0.5;
    black = 0.0;
    return "BLACK = OFF";
  }
  black = (clamp(x, 0.0, 1.0) - 0.5) * 20.0;
  return "BLACK = " + str(black);
}

cv::Point2d black() {
  auto &black = config->black;
  return {clamp(black / 20.0 + 0.5, 0.0, 1.0)};
}

typedef struct {
  Tile *fps, *exp, *gain, *gamma, *black;
} Sliders;

void rerender(Sliders &sliders) {
  sliders.fps->use(fps());
  sliders.exp->use(exp());
  sliders.gain->use(gain());
  sliders.gamma->use(gamma());
  sliders.black->use(black());
}

#undef LOGNAME
#define LOGNAME "[task:tune] "

void tasks::tune(Context &ctx) {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int N = 5;
  const int btn_w = w / 4, btn_h = btn_w / 2;
  const int img_h = std::min(w / 2, h / 2);
  const int slider_h = std::min((h - img_h - btn_h) / N, btn_h);
  const int img_margin = (h - img_h - N * slider_h - btn_h) / 2;
  int y = img_margin;
  // Image tiles
  Tile wide({0, y, w / 2, img_h}, pad);
  wide.mbox({0, 0, 1, 0.75}).tbox({0, 0.825, 1, 0.1}).text("Wide Camera");
  Tile fovea({w / 2, y, w / 2, img_h}, pad);
  fovea.mbox({0, 0, 1, 0.75}).tbox({0, 0.825, 1, 0.1}).text("Zoom Camera");
  // Create tiles
  y += img_h + img_margin;
  Tile slider_fps(cv::Rect{0, y, w, slider_h}, pad, TileMode::X_SLIDER);
  slider_fps.use([](Tile &tile, bool) {
    tile.text(FPS(tile.val.x));
    config->updated = true;
  });
  y += slider_h;
  Tile slider_exp(cv::Rect{0, y, w, slider_h}, pad, TileMode::X_SLIDER);
  slider_exp.use([](Tile &tile, bool) {
    tile.text(EXP(tile.val.x));
    config->updated = true;
  });
  y += slider_h;
  Tile slider_gain(cv::Rect{0, y, w, slider_h}, pad, TileMode::X_SLIDER);
  slider_gain.use([](Tile &tile, bool) {
    tile.text(GAIN(tile.val.x));
    config->updated = true;
  });
  y += slider_h;
  Tile slider_gamma(cv::Rect{0, y, w, slider_h}, pad, TileMode::X_SLIDER);
  slider_gamma.use([](Tile &tile, bool) {
    tile.text(GAMMA(tile.val.x));
    config->updated = true;
  });
  y += slider_h;
  Tile slider_black(cv::Rect{0, y, w, slider_h}, pad, TileMode::X_SLIDER);
  slider_black.use([](Tile &tile, bool) {
    tile.text(BLACK(tile.val.x));
    config->updated = true;
  });
  y += slider_h;
  Sliders sliders = {
      &slider_fps, &slider_exp, &slider_gain, &slider_gamma, &slider_black,
  };
  rerender(sliders);
  int x = 0;
  auto back_btn = GUI::back_btn(cv::Rect{x, y, btn_w, btn_h}, pad);
  x += btn_w;
  Tile reset_btn(cv::Rect{x, y, btn_w, btn_h}, pad, TileMode::BUTTON);
  x += btn_w;
  Tile sync_btn(cv::Rect{x, y, btn_w, btn_h}, pad, TileMode::BUTTON);
  x += btn_w;
  Tile zebra_btn(cv::Rect{x, y, btn_w, btn_h}, pad, TileMode::BUTTON);

  reset_btn.text("RESET").use([&](Tile &, bool) {
    if (config == &global::config.wide) {
      *config = global::default_config.wide;
    } else if (config == &global::config.fovea) {
      *config = global::default_config.fovea;
    } else {
      std::cerr << LOGNAME "unknown config address, aborting..." << std::endl;
      return;
    }
    config->updated = true;
    rerender(sliders);
  });

  sync_btn.text("SYNC").use([&](Tile &, bool) {
    if (config == &global::config.wide) {
      *config = global::config.fovea;
    } else if (config == &global::config.fovea) {
      *config = global::config.wide;
    } else {
      std::cerr << LOGNAME "unknown config address, aborting..." << std::endl;
      return;
    }
    config->updated = true;
    rerender(sliders);
  });

  zebra_btn.text("ZEBRA").use([](Tile &tile, bool) {
    flag_highlight = !flag_highlight;
    if (flag_highlight) {
      tile.style.normal.fill = color::cyan(0.6) + color::mono(0.2);
      tile.style.text.color = color::mono(0.2);
    } else {
      tile.style.normal.fill = graphics::default_style.normal.fill;
      tile.style.text.color = graphics::default_style.text.color;
    }
    tile.render(true);
    tile.text("ZEBRA");
  });

  wide.use([&](Tile &tile, bool state_change) {
    if (!state_change || tile.is_active())
      return;
    if (config == &global::config.wide)
      return;
    config = &global::config.wide;
    rerender(sliders);
    tile.fill();
    fovea.wipe();
  });

  fovea.use([&](Tile &tile, bool state_change) {
    if (!state_change || tile.is_active())
      return;
    if (config == &global::config.fovea)
      return;
    config = &global::config.fovea;
    rerender(sliders);
    tile.fill();
    wide.wipe();
  });

  if (config == &global::config.wide)
    wide.fill();
  else if (config == &global::config.fovea)
    fovea.fill();

  std::vector tiles = {
      &wide,        &fovea,        &slider_fps,   &slider_exp,
      &slider_gain, &slider_gamma, &slider_black, &back_btn,
      &reset_btn,   &sync_btn,     &zebra_btn,
  };
  // Launch worker threads
  std::vector<global::ThreadInfo> threads;
  MatPipe wide_pipe, fovea_pipe;
  threads.push_back({
      "render-helper:wide",
      render_helper(ctx.cap_wide, wide_pipe),
  });
  threads.push_back({
      "mat-renderer:wide",
      GUI::mat_renderer(wide_pipe, wide),
  });
  threads.push_back({
      "render-helper:fovea",
      render_helper(ctx.cap_fovea, fovea_pipe),
  });
  threads.push_back({
      "mat-renderer:fovea",
      GUI::mat_renderer(fovea_pipe, fovea),
  });
  // Render loop
  try {
    while (!global::flag_term) {
      auto pos = fb.wait_pointer(true);
      for (auto &tile : tiles)
        tile->handle(pos);
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
