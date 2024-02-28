#include <iostream>
#include <opencv2/calib3d.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <thread>

#include "GUI.h"
#include "global.h"
#include "graphics/shared.h"
#include "outfile.h"
#include "tasks.h"
#include "threading/fast_io.h"
#include "util/fmt.h"

#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>

using namespace graphics;

#undef LOGNAME
#define LOGNAME "[task:checker:info] "

#define FMT(X) (fmt((X), 2, 0, '0', false))

typedef struct CheckerInfo {
  bool found;
  cv::Size size;
  std::vector<cv::Point2f> corners;
  void dump(outfile::Item **out, std::string name, cv::Size frameSize) const {
    if (!found)
      return;
    std::stringstream ss;
    ss << "T = WIDE; "                       //
       << "H = " << frameSize.height << "; " //
       << "W = " << frameSize.width << "; "  //
       << "N1 = " << size.height << "; "     //
       << "N2 = " << size.width << "; "      //
       << "Corners =";
    for (auto &el : corners)
      ss << " " << el.x << "," << el.y;
    ss << std::endl;
    std::cout << ss.str();
    if ((*out) != nullptr)
      *((*out)->fs) << ss.str();
    else
      std::cerr << LOGNAME << "Error: outfile not initialized for " << name
                << std::endl;
  }
  void draw(cv::Mat &img) const {
    if (found) {
      int i = 0;
      for (auto &corner : corners) {
        int row = (i++) / size.width;
        cv::circle(img, corner, 12,
                   color::hsla(static_cast<double>(row) / size.height, 1, 0.5),
                   4);
      }
    }
  }
} CheckerInfo;

typedef threading::FastIO<CheckerInfo> CheckerPipe;

#undef LOGNAME
#define LOGNAME "[task:checker:finder] "

const cv::TermCriteria criteria(                        //
    cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, //
    40,                                                 //
    0.001                                               //
);

std::thread checker_finder(MatPipe &frame_in, unsigned int &N,
                           CheckerPipe &checker_out, bool &enable) {
  return std::thread([&]() {
    try {
      auto frame = frame_in.read();
      while (!global::flag_term) {
        frame_in.next(frame, true);
        if (enable) {
          CheckerInfo info = {false, cv::Size(N, N), {}};
          info.found = cv::findChessboardCorners(
              *frame, info.size, info.corners,
              cv::CALIB_CB_ADAPTIVE_THRESH + cv::CALIB_CB_NORMALIZE_IMAGE +
                  cv::CALIB_CB_LARGER);
          if (info.found)
            cv::cornerSubPix(*frame, info.corners, {11, 11}, {-1, -1},
                             criteria);
          checker_out.write(info);
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    frame_in.close();
    checker_out.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:checker:renderer-wide] "

std::thread wide_renderer(Tile &tile, MatPipe &view_in,
                          CheckerPipe &checker_info_in, MatPipe &frame_out,
                          bool &cap, bool &enabled, outfile::Item **out) {
  return std::thread([&, out]() {
    try {
      int count = 0;
      std::vector<cv::Point2f> corners;
      tile.auto_raster = false;
      auto frame = view_in.read();
      while (!global::flag_term) {
        view_in.next(frame, true);
        cv::Mat view;
        cv::cvtColor(*frame, view, cv::COLOR_BGRA2GRAY);
        cv::Mat disp;
        cv::cvtColor(view, disp, cv::COLOR_GRAY2BGRA);
        frame_out.write(view);
        if (enabled) {
          auto info = checker_info_in.read();
          if (info != nullptr && info->found) {
            if (cap) {
              info->dump(out, "WIDE", disp.size());
              cv::imwrite("checker_wide_" + FMT(++count) + ".png", *frame);
              cap = false;
            }
            info->draw(disp);
          }
          tile.text(FMT(count) + " Samples Saved").use(disp).raster();
        } else {
          tile.use(disp).raster();
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    view_in.close();
    checker_info_in.close();
    frame_out.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:checker:renderer-fovea] "

std::thread fovea_renderer(Tile &tile, FoveaPipe &view_in,
                           CheckerPipe &checker_info_in, MatPipe &frame_out,
                           bool &cap, bool &enabled, outfile::Item **out) {
  return std::thread([&, out]() {
    try {
      int count = 0;
      std::vector<cv::Point2f> corners;
      tile.auto_raster = false;
      auto fovea = view_in.read();
      while (!global::flag_term) {
        view_in.next(fovea, true);
        cv::Mat view;
        cv::cvtColor(fovea->mat, view, cv::COLOR_BGRA2GRAY);
        cv::Mat disp;
        cv::cvtColor(view, disp, cv::COLOR_GRAY2BGRA);
        frame_out.write(view);
        auto info = checker_info_in.read();
        if (enabled) {
          if (info != nullptr && info->found) {
            if (cap) {
              info->dump(out, "FOVEA", disp.size());
              cv::imwrite("checker_fovea_" + FMT(++count) + ".png", fovea->mat);
              cap = false;
            }
            info->draw(disp);
          }
          tile.text(FMT(count) + " Samples Saved").use(disp).raster();
        } else {
          tile.use(disp).raster();
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    view_in.close();
    checker_info_in.close();
    frame_out.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:checker] "

void tasks::checker(Context &ctx) {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4;
  const int content_h = h - btn_h;
  const int img_h = content_h / 2;
  // Shared variable, mapping form [0, 1] to [2, 20]
  unsigned int N = 7;
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
  Tile size_slider(cv::Rect{x, y, btn_w * 3, btn_h}, pad);
  x += btn_w;
  // Shared variables across threads
  outfile::Item *out_wide = nullptr, *out_fovea = nullptr;
  bool flag_wide_cap = false, flag_fovea_cap = false;
  bool flag_wide_checker_enable = false, flag_fovea_checker_enable = false;
  std::vector<Tile *> tiles = {
      &wide_tile //
           .as(TileMode::BUTTON)
           .text("Click to Start Checkerboard Detector")
           .use([&flag_wide_cap, &flag_wide_checker_enable,
                 &out_wide](Tile &tile, bool) {
             if (out_wide == nullptr) {
               out_wide = new outfile::Item("wide.txt");
               outfile::items.push_back(out_wide);
             }
             flag_wide_checker_enable = true;
             flag_wide_cap = true;
           }),
      &fovea_tile //
           .as(TileMode::BUTTON)
           .text("Click to Start Checkerboard Detector")
           .use([&flag_fovea_cap, &flag_fovea_checker_enable,
                 &out_fovea](Tile &tile, bool) {
             if (out_fovea == nullptr) {
               out_fovea = new outfile::Item("fovea.txt");
               outfile::items.push_back(out_fovea);
             }
             flag_fovea_checker_enable = true;
             flag_fovea_cap = true;
           }),
      &back_btn,
      &size_slider //
           .as(TileMode::X_SLIDER)
           .use([&N](Tile &tile, bool) {
             N = std::rint(tile.val.x * 18 + 0.5) + 2;
             tile.text("N = " + std::to_string(N));
           })
           .use(cv::Point2d{.25, 0}),
  };
  // Launch worker threads
  std::vector<global::ThreadInfo> threads;
  MatPipe frame_wide, frame_fovea;
  CheckerPipe checker_wide, checker_fovea;
  threads.push_back({
      "renderer/wide",
      wide_renderer(wide_tile, ctx.cap_wide, checker_wide, frame_wide,
                    flag_wide_cap, flag_wide_checker_enable, &out_wide),
  });
  threads.push_back({
      "renderer/fovea",
      fovea_renderer(fovea_tile, ctx.cap_fovea, checker_fovea, frame_fovea,
                     flag_fovea_cap, flag_fovea_checker_enable, &out_fovea),
  });
  threads.push_back({
      "finder/wide",
      checker_finder(frame_wide, N, checker_wide, flag_wide_checker_enable),
  });
  threads.push_back({
      "finder/fovea",
      checker_finder(frame_fovea, N, checker_fovea, flag_fovea_checker_enable),
  });
  // Render loop
  try {
    ctx.mems_pos.flush().write({0.0, 0.0});
    while (!global::flag_term) {
      auto pos = fb.wait_pointer();
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
