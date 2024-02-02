#include <iostream>
#include <memory>
#include <mutex>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <thread>

#include "GUI.h"
#include "global.h"
#include "tasks.h"

#include <calib/calib.h>
#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>
#include <util/clamp.h>

using namespace graphics;

typedef struct Tracker {
  const uint8_t id;
  const std::string name;
  Tile *const tile;
  cv::Rect roi;
  cv::Point2d mems_volt;
  std::mutex lock;
  bool valid = false;
  std::thread *thread = nullptr;
  void wait() {
    if (thread != nullptr) {
      valid = false;
      thread->join();
      delete thread;
      thread = nullptr;
    }
  }
  void update_mems_pos(const cv::Mat &img) {
    cv::Point2d center = (roi.tl() + roi.br()) / 2.0;
    center.x /= img.cols;
    center.y /= img.rows;
    auto volt = calib::cvt(calib::PtoV, center - calib::shift);
    mems_volt = {volt.x * 180.0 - 90.0, volt.y * 180.0 - 90.0};
  }
} Tracker;

#undef LOGNAME
#define LOGNAME "[task:track:pos-watcher] "

std::thread pos_watcher(Context &ctx, std::vector<Tracker *> &trackers) {
  return std::thread([&]() {
    std::cerr << LOGNAME "started." << std::endl;
    try {
      auto wide = ctx.cap_wide.read();
      while (!global::flag_term) {
        ctx.cap_wide.next(wide, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        for (auto &tracker : trackers) {
          if (!tracker->valid)
            continue;
          std::lock_guard lock(tracker->lock);
          const auto &volt = tracker->mems_volt;
          ctx.mems_pos.write({volt.x, volt.y, tracker->id});
        }
        while (!ctx.mems_pos.empty())
          ctx.mems_pos.wait_read();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:track:tracker] "

void tracker(Context &ctx, Tracker &self) {
  self.valid = true;
  self.thread = new std::thread([&]() {
    std::cerr << LOGNAME << "<" << self.name << "> started." << std::endl;
    try {
      self.tile->style.normal.outline.weight = 4;
      self.tile->text("").raster();
      auto tracker = cv::TrackerKCF::create();
      std::shared_ptr<const cv::Mat> frame = nullptr;
      ctx.cap_wide.next(frame, true);
      cv::Mat img;
      cv::cvtColor(*frame, img, cv::COLOR_BGRA2BGR);
      tracker->init(*frame, self.roi);
      while (!global::flag_term && self.valid) {
        ctx.cap_wide.next(frame, true);
        std::lock_guard lock(self.lock);
        cv::cvtColor(*frame, img, cv::COLOR_BGRA2BGR);
        if (tracker->update(img, self.roi))
          self.update_mems_pos(img);
        else
          break;
      }
      self.valid = false;
      self.tile->style.normal.outline.weight = 0;
      self.tile->use(cv::Mat()).text(self.name).raster();
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    std::cerr << LOGNAME << "<" << self.name << "> exited." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:track:wide-renderer] "

std::thread wide_renderer(MatPipe &mat_pipe, Tile &tile,
                          std::vector<Tracker *> &trackers) {
  return std::thread([&]() {
    try {
      tile.auto_raster = false;
      auto frame = mat_pipe.read();
      cv::Mat disp;
      while (!global::flag_term) {
        mat_pipe.next(frame, true);
        disp = frame->clone();
        for (auto &tracker : trackers) {
          if (!tracker->valid)
            continue;
          try {
            const auto &roi = tracker->roi;
            const auto &color = tracker->tile->style.normal.outline.color;
            cv::rectangle(disp, roi, color, 4);
          }
          CATCH_ASSERT(LOGNAME);
        }
        tile.use(disp).raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    mat_pipe.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:track:fovea-renderer] "

std::thread fovea_renderer(FoveaPipe &fovea_pipe,
                           std::vector<Tracker *> &trackers) {
  unsigned n = 0;
  for (auto &tracker : trackers) {
    auto &tile = *tracker->tile;
    tile.auto_raster = false;
    tile.style.text.color = color::mono(1.0, 0.4);
    tile.style.normal.outline.color = color::hsla(n++ / 6.0, 1.0, 0.5);
    tile.text(tracker->name).raster();
  }
  return std::thread([&]() {
    try {
      auto fovea = fovea_pipe.read();
      cv::Mat disp;
      while (!global::flag_term) {
        fovea_pipe.next(fovea, true);
        const auto &tag = fovea->tag;
        if (tag > 0 && tag <= trackers.size()) {
          auto tracker = trackers.at(tag - 1);
          if (tracker->valid)
            tracker->tile->use(fovea->mat).raster();
        }
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    fovea_pipe.close();
    std::cerr << LOGNAME "terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:track] "

void tasks::track(Context &ctx) {
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  canvas.clear();
  // Vector of all renderer threads
  std::vector<global::ThreadInfo> threads;
  // Prepare tiles for interaction
  const int w = fb.shape().width, h = fb.shape().height;
  const int pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4;
  const int content_h = h - btn_h;
  const int img_h1 = std::min(content_h / 4, w / 4);
  const int img_h2 = h - 2 * img_h1 - btn_h;
  int x, y;
  // Create tiles
  x = 0;
  y = 0;
  // Tiles - row 1
  Tile tile_a(cv::Rect{x, y, w / 3, img_h1}, pad);
  x += w / 3;
  Tile tile_b(cv::Rect{x, y, w / 3, img_h1}, pad);
  x += w / 3;
  Tile tile_c(cv::Rect{x, y, w / 3, img_h1}, pad);
  // Tiles - row 2
  x = 0;
  y += img_h1;
  Tile tile_d(cv::Rect{x, y, w / 3, img_h1}, pad);
  x += w / 3;
  Tile tile_e(cv::Rect{x, y, w / 3, img_h1}, pad);
  x += w / 3;
  Tile tile_f(cv::Rect{x, y, w / 3, img_h1}, pad);
  // List of all tiles
  std::vector<Tracker *> trackers;
  trackers.push_back(new Tracker{1, "A", &tile_a});
  trackers.push_back(new Tracker{2, "B", &tile_b});
  trackers.push_back(new Tracker{3, "C", &tile_c});
  trackers.push_back(new Tracker{4, "D", &tile_d});
  trackers.push_back(new Tracker{5, "E", &tile_e});
  trackers.push_back(new Tracker{6, "F", &tile_f});
  threads.push_back({
      "task/track/pos-watcher",
      pos_watcher(ctx, trackers),
  });
  threads.push_back({
      "task/track/fovea-render",
      fovea_renderer(ctx.cap_fovea, trackers),
  });
  // Tiles (wide view)
  x = 0;
  y += img_h1;
  Tile tile_wide(cv::Rect{x, y, w, img_h2}, pad);
  threads.push_back({
      "task/track/wide-renderer",
      wide_renderer(ctx.cap_wide, tile_wide, trackers),
  });
  // Buttons
  x = 0;
  y += img_h2;
  auto back_btn = GUI::back_btn(cv::Rect{0, y, btn_w, btn_h}, pad);
  x += btn_w;
  Tile reset_btn(cv::Rect{x, y, btn_w, btn_h}, pad);
  x += btn_w;
  Tile notes(cv::Rect{x, y, btn_w * 2, btn_h}, pad);
  notes.style.bg = color::mono(0.0);
  Tracker *preview = nullptr;
  std::vector<Tile *> tiles = {
      &tile_a.use([&trackers](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active())
          trackers[0]->wait();
        trackers[0]->tile->use(cv::Mat());
      }),
      &tile_b.use([&trackers](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active())
          trackers[1]->wait();
        trackers[1]->tile->use(cv::Mat());
      }),
      &tile_c.use([&trackers](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active())
          trackers[2]->wait();
        trackers[2]->tile->use(cv::Mat());
      }),
      &tile_d.use([&trackers](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active())
          trackers[3]->wait();
        trackers[3]->tile->use(cv::Mat());
      }),
      &tile_e.use([&trackers](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active())
          trackers[4]->wait();
        trackers[4]->tile->use(cv::Mat());
      }),
      &tile_f.use([&trackers](Tile &tile, bool state_change) {
        if (state_change && !tile.is_active())
          trackers[5]->wait();
        trackers[5]->tile->use(cv::Mat());
      }),
      &tile_wide.use([&](Tile &tile, bool state_change) {
        auto wide = ctx.cap_wide.read();
        if (wide == nullptr) {
          std::cerr << LOGNAME "No wide frame available." << std::endl;
          return;
        }
        if (preview == nullptr) {
          for (auto &el : trackers) {
            if (!el->valid) {
              preview = el;
              std::cerr << LOGNAME "Allocating tile " << preview->name
                        << std::endl;
              preview->wait();
              preview->tile->style.normal.outline.weight = 4;
              break;
            }
          }
          if (preview == nullptr) {
            std::cerr << LOGNAME "No tracker available for allocation."
                      << std::endl;
            return;
          }
        }
        auto volt = calib::cvt(calib::PtoV, tile.val - calib::shift);
        volt.x = clamp(volt.x, 0.0, 1.0);
        volt.y = clamp(volt.y, 0.0, 1.0);
        auto pos = calib::cvt(calib::VtoP, volt) + calib::shift;
        {
          std::lock_guard lock(preview->lock);
          preview->roi =
              calib::roi(pos, wide->size(), global::config.lens.scale);
          preview->update_mems_pos(*wide);
          preview->valid = true;
        }
        if (!tile.is_active() && state_change) {
          tracker(ctx, *preview);
          preview = nullptr;
        }
      }),
      &back_btn,
      &reset_btn.as(TileMode::BUTTON).text("RESET").use([&](Tile &tile, bool) {
        for (auto &el : trackers)
          el->wait();
      }),
      &notes.text("Current task: track"),
  };
  // Render loop
  try {
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
  for (auto &el : trackers) {
    el->wait();
    delete el;
  }
  for (auto &el : threads) {
    std::cerr << LOGNAME "waiting for " << el.name << std::endl;
    el.thread.join();
  }
  std::cerr << LOGNAME "terminated." << std::endl;
}
