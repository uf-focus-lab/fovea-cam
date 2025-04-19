#include <iostream>
#include <math.h>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <sstream>
#include <thread>

#include "GUI.h"
#include "global.h"
#include "mems/mems.h"
#include "outfile.h"
#include "tasks.h"
#include "util/fmt.h"
#include "util/time.h"

#include <calib/calib.h>
#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>
#include <util/clamp.h>

using namespace graphics;

typedef struct DispatchCmd {
  bool active = false, updated = false;
  cv::Point2d volt;
} DispatchCmd;

void send(Context &ctx, cv::Point2d volt, uint8_t tag = 0) {
  ctx.mems_pos.flush().write({volt.x, volt.y, tag});
}

#undef LOGNAME
#define LOGNAME "[task:stabilize:dispatcher] "

std::thread dispatcher(Context &ctx, DispatchCmd &user, DispatchCmd &tracker) {
  return std::thread([&]() {
    std::cerr << LOGNAME "started." << std::endl;
    try {
      while (!global::flag_term) {
        if (user.active) {
          if (user.updated) {
            user.updated = false;
            send(ctx, user.volt);
          }
        } else if (tracker.active) {
          if (tracker.active) {
            if (tracker.updated) {
              tracker.updated = false;
              send(ctx, tracker.volt, 255);
            }
          }
        } else if (ctx.mems_pos.empty()) {
          send(ctx, {0.5, 0.5}, 1);
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    ctx.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:stabilize:tracker] "

cv::Point2d center_raw, center_pred, velocity;

static inline void predict(DispatchCmd &cmd, cv::Size size, double dt,
                           double &advance) {
  // Advance MEMS position based on previous ROI and velocity
  center_pred = center_raw + velocity * (dt + advance);
  // Convert to volt
  cv::Point2d pos(center_pred.x / size.width, center_pred.y / size.height);
  cmd.volt = calib::pos2volt(pos);
  cmd.updated = true;
  std::stringstream ss;
}

std::string fmt_point(bool sign, cv::Point2d p) {
  return ", " + fmt(p.x, 4, 2, '0', sign) + //
         ", " + fmt(p.y, 4, 2, '0', sign);
}

std::string fmt_point(bool sign, bool) {
  return sign ? ",         ,         " : ",        ,        ";
}

#define PRINT_TRAJECTORY(t, c, v, p)                                           \
  fmt(t, 8, 2, '0', false) << fmt_point(false, c) << fmt_point(true, v)        \
                           << fmt_point(false, p) << std::endl

std::thread tracker(Context &ctx, DispatchCmd &cmd, double &advance,
                    double &decay) {
  return std::thread([&]() {
    std::cerr << LOGNAME << "started." << std::endl;
    auto out_item = new outfile::Item("trajectory.csv");
    outfile::items.push_back(out_item);
    out_item->keep = false;
    auto &out = *out_item->fs;
    out << "     t     ,"
           "   Cx   ,   Cy   ,"
           "   Vx   ,   Vy   ,"
           "   Px   ,   Py   "
        << std::endl;
    const auto t_origin = Time::now();
    try {
      auto frame = ctx.cap_wide.read();
      auto fovea = ctx.cap_fovea.read();
      cv::Ptr<cv::TrackerKCF> tracker = nullptr;
      cv::Rect roi;
      double t_last_update, dt;
      while (!global::flag_term) {
        if (cmd.active) {
          if (tracker != nullptr) {
            // Tracker has been initialized
            while (true) {
              bool next_frame = ctx.cap_wide.next(frame, false);
              auto t = Time::us(t_origin) / 1000.0;
              dt = t - t_last_update;
              if (next_frame) {
                // Update ROI
                cv::Mat mat;
                cv::cvtColor(*frame, mat, cv::COLOR_BGRA2BGR);
                if (tracker->update(mat, roi)) {
                  t_last_update = t;
                  cv::Point2d center = (roi.tl() + roi.br()) / 2.0;
                  auto dx = center - center_raw;
                  center_raw = center;
                  // Compute accumulated velocity
                  velocity = decay * velocity + (1 - decay) * dx / dt;
                  // Predict next MEMS position
                  predict(cmd, frame->size(), 0, advance);
                  out << PRINT_TRAJECTORY(t, center_raw, velocity, center_pred);
                } else if (dt > 1000) {
                  // Lost tracking for more than 1 second, abort
                  cmd.active = false;
                  break;
                }
              } else if (ctx.mems_pos.empty()) {
                predict(cmd, frame->size(), dt, advance);
                out << PRINT_TRAJECTORY(t, false, false, center_pred);
                // std::this_thread::sleep_for(std::chrono::milliseconds(1));
              }
            }
          } else {
            // Initialize tracker
            ctx.cap_wide.next(frame, true);
            ctx.cap_fovea.next(fovea, true);
            auto pos = calib::volt2pos(fovea->volt());
            roi = calib::roi(pos, frame->size(), global::config.lens.scale);
            tracker = cv::TrackerKCF::create();
            tracker->init(*frame, roi);
            // Reset predictor
            t_last_update = Time::ms();
            center_pred = center_raw = (roi.tl() + roi.br()) / 2.0;
            velocity = {0, 0};
          }
        } else if (tracker != nullptr) {
          tracker = nullptr;
          cmd.updated = false;
          auto t = Time::us(t_origin) / 1000.0;
          out << PRINT_TRAJECTORY(t, false, false, false);
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    std::cerr << LOGNAME << "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:stabilize:wide-renderer] "

std::thread wide_renderer(Context &ctx, Tile &tile, DispatchCmd &user,
                          DispatchCmd &tracker) {
  return std::thread([&]() {
    try {
      tile.auto_raster = false;
      auto frame = ctx.cap_wide.read();
      auto fovea = ctx.cap_fovea.read();
      cv::Mat disp;
      while (!global::flag_term) {
        ctx.cap_wide.next(frame, true);
        ctx.cap_fovea.next(fovea, false);
        disp = frame->clone();
        if (fovea != nullptr && (user.active || tracker.active)) {
          try {
            auto pos = calib::volt2pos(fovea->volt());
            auto roi =
                calib::roi(pos, frame->size(), global::config.lens.scale);
            roi.x -= 1;
            roi.y -= 1;
            roi.width += 2;
            roi.height += 2;
            if (fovea->tag == 255) {
              cv::rectangle(disp, roi, color::red(), 2);
            } else {
              cv::rectangle(disp, roi, color::blue(), 4);
            }
            if (tracker.active) {
              auto &A = center_raw, B = A + velocity * 200;
              cv::drawMarker(disp, A, color::red(), cv::MARKER_SQUARE, 12, 12);
              cv::circle(disp, B, 12, color::red(), -1);
              cv::line(disp, A, B, color::yellow(), 4);
            }
          }
          CATCH_ASSERT(LOGNAME);
        }
        tile.use(disp).raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    ctx.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:stabilize] "

void tasks::stabilize(Context &ctx) {
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
  Tile fovea_tile(cv::Rect{0, y, w, img_h}, pad);
  fovea_tile.tbox({0, 0.9, 1, 0.1});
  fovea_tile.style.text.inset = 0.3;
  fovea_tile.style.text.bg = color::mono(0, 0.2);
  fovea_tile.style.text.color = color::yellow();
  y += img_h;
  Tile wide_tile(cv::Rect{0, y, w, img_h}, pad);
  wide_tile.tbox({0, 0.9, 1, 0.1});
  wide_tile.style.text.inset = 0.3;
  wide_tile.style.text.bg = color::mono(0, 0.2);
  wide_tile.style.text.color = color::cyan();
  y += img_h;
  auto back_btn = GUI::back_btn(cv::Rect{x, y, btn_w, btn_h}, pad);
  x += btn_w;
  Tile decay_slider(cv::Rect{x, y, btn_w * 3 / 2, btn_h}, pad);
  x += btn_w * 3 / 2;
  Tile pred_slider(cv::Rect{x, y, btn_w * 3 / 2, btn_h}, pad);
  DispatchCmd user, tracker_cmd;
  double pred, decay;
  std::vector<Tile *> tiles = {
      &fovea_tile.use([&tracker_cmd](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active()) {
          tracker_cmd.active = false;
        }
      }),
      &wide_tile.use([&user, &tracker_cmd](Tile &tile, bool state_change) {
        auto active = tile.is_active();
        if (state_change) {
          user.active = active;
          tracker_cmd.active = !active;
        }
        if (active) {
          user.volt = calib::pos2volt(tile.val);
          user.updated = true;
        }
      }),
      &back_btn,
      &decay_slider.as(TileMode::X_SLIDER)
           .use([&decay](Tile &tile, bool) {
             decay = sqrt(clamp(tile.val.x, 0., 0.99));
             tile.text("Decay " + fmt(decay * 100, 2, 2, ' ', false) + "%");
           })
           .use(cv::Point2d{0.81, 0}),
      &pred_slider.as(TileMode::X_SLIDER)
           .use([&pred](Tile &tile, bool) {
             pred = 100 * clamp(tile.val.x, 0., 1.);
             tile.text("Pred " + fmt(pred, 3, 2, ' ', false) + " ms");
           })
           .use(cv::Point2d{1. / 3., 0}),
  };
  // Launch worker threads
  std::vector<global::ThreadInfo> threads;
  threads.push_back({
      "renderer/fovea",
      GUI::fovea_renderer(ctx.cap_fovea, fovea_tile),
  });
  threads.push_back({
      "renderer/wide",
      wide_renderer(ctx, wide_tile, user, tracker_cmd),
  });
  threads.push_back({
      "dispatcher",
      dispatcher(ctx, user, tracker_cmd),
  });
  threads.push_back({
      "tracker",
      tracker(ctx, tracker_cmd, pred, decay),
  });
  // Render loop
  try {
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
