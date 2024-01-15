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
  cv::Mat mask_low, mask_high;
  cv::inRange(gray, {0}, {1}, mask_low);
  cv::inRange(gray, {254}, {255}, mask_high);
  std::vector<cv::Mat> channels;
  channels.push_back(gray);
  channels.push_back(gray);
  channels.push_back(gray);
  channels.push_back(cv::Mat(gray.rows, gray.cols, CV_8UC1, {128}));
  channels.at(0 /* BLUE */)(mask_low) = 255;
  channels.at(1 /* RED */)(mask_high) = 255;
  cv::Mat region = mask_low | mask_high;
  channels.back(/* ALPHA */)(region) = 255;
  cv::Mat result;
  cv::merge(channels, result);
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
    gain = clamp<double>(x, 0, 1) * 40.0;
  else
    gain = 0.0;
  return "GAIN = " + str(gain);
}

cv::Point2d gain() {
  auto &gain = config->gain;
  return {clamp(gain, 0.0, 40.0) / 40.0};
}

class ClickableImg {
public:
  Tile outer, image, text;
  ClickableImg(cv::Rect box, int pad, float v_div = 0.75)
      : outer(box, pad),
        image(cv::Rect{box.x + pad, box.y + pad, box.width - 2 * pad,
                       GUI::mul(box.height - 2 * pad, v_div)}),
        text(cv::Rect{
            box.x + pad, box.y + pad + GUI::mul(box.height - 2 * pad, v_div),
            box.width - 2 * pad, GUI::mul(box.height - 2 * pad, 1 - v_div)}){};
  void to(std::vector<Tile *> &tiles) {
    tiles.push_back(&text);
    tiles.push_back(&image);
    tiles.push_back(&outer);
  };
};

#undef LOGNAME
#define LOGNAME "[task:tune] "

void tasks::tune(Context &ctx) {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int btn_w = w / 4, btn_h = btn_w / 2;
  const int img_h = std::min(w / 2, h / 2);
  // Image tiles
  ClickableImg wide({0, 0, w / 2, img_h}, pad);
  ClickableImg fovea({w / 2, 0, w / 2, img_h}, pad);
  // Create tiles
  int y = img_h, slider_h = (h - img_h - btn_h) / 3;
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
  std::vector sliders = {
      &slider_fps,
      &slider_exp,
      &slider_gain,
  };
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
    for (auto &slider : sliders)
      slider->render(true);
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
    for (auto &slider : sliders)
      slider->render(true);
  });

  zebra_btn.text("ZEBRA").use([](Tile &tile, bool) {
    flag_highlight = !flag_highlight;
    tile.style.normal.fill =
        flag_highlight ? color::cyan(0.5) : graphics::default_style.normal.fill;
  });

  wide.text.text("Wide Camera");
  wide.outer.use([&](Tile &tile, bool state_change) {
    if (!state_change || tile.is_active())
      return;
    if (config == &global::config.wide)
      return;
    config = &global::config.wide;
    for (auto &slider : sliders)
      slider->render(true);
  });

  fovea.text.text("Zoom Camera");
  fovea.outer.use([&](Tile &tile, bool state_change) {
    if (!state_change || tile.is_active())
      return;
    if (config == &global::config.fovea)
      return;
    config = &global::config.fovea;
    for (auto &slider : sliders)
      slider->render(true);
  });

  std::vector tiles = {
      &slider_fps, &slider_exp, &slider_gain, &back_btn,
      &reset_btn,  &sync_btn,   &zebra_btn,
  };
  std::vector tiles_interact(tiles);
  wide.to(tiles);
  fovea.to(tiles);
  tiles_interact.push_back(&wide.outer);
  tiles_interact.push_back(&fovea.outer);
  // Launch worker threads
  std::vector<global::ThreadInfo> threads;
  MatPipe wide_pipe, fovea_pipe;
  threads.push_back({
      "render-helper:wide",
      render_helper(ctx.cap_wide, wide_pipe),
  });
  threads.push_back({
      "mat-renderer:wide",
      GUI::mat_renderer(wide_pipe, wide.image),
  });
  threads.push_back({
      "render-helper:wide",
      render_helper(ctx.cap_wide, wide_pipe),
  });
  threads.push_back({
      "mat-renderer:wide",
      GUI::mat_renderer(wide_pipe, wide.image),
  });
  // Render loop
  try {
    while (!global::flag_term) {
      auto pos = fb.wait_pointer(true);
      for (auto &tile : tiles_interact)
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
