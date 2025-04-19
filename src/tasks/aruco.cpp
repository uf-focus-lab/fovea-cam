#include <iostream>
#include <opencv2/aruco.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/aruco_dictionary.hpp>
#include <opencv2/opencv.hpp>
#include <sstream>
#include <string>
#include <thread>

#include "GUI.h"
#include "global.h"
#include "graphics/shared.h"
#include "outfile.h"
#include "tasks.h"

#include <calib/calib.h>
#include <graphics/canvas.h>
#include <graphics/tile.h>
#include <util/assert.h>
#include <util/clamp.h>
#include <util/fmt.h>

using namespace graphics;

typedef struct {
  // ArUco marker ID embedded in the image
  int id;
  // Center Position of the detected marker
  // (0, 0) is the center of the image
  cv::Point2d pos;
  // Corners of the detected marker
  std::vector<cv::Point2f> corners;
  // Current position of the MEMS mirror (optional)
  cv::Point2d volt;
} ArUcoInfo;

typedef threading::FastIO<std::vector<ArUcoInfo>> ArUcoPipe;

#undef LOGNAME
#define LOGNAME "[task:aruco:PID-controller] "
// If the center of the marker is within this distance from the center of the
// image, no correction will be performed.
static const double max_output = 90.0;
static double kp = 0.006;

void normalize(double &volt, double max = max_output) {
  if (volt > max)
    volt = max;
  else if (volt < -max)
    volt = -max;
}

std::thread pid_controller(Context &ctx, ArUcoPipe &aruco_wide_in,
                           ArUcoPipe &aruco_fovea_in, const bool &enable) {
  return std::thread([&]() {
    outfile::Item *out = nullptr;
    try {
      int prev_id = 0;
      std::string info_volt = "N/A", info_wide = "N/A", info_fovea = "N/A";
      ctx.mems_pos.write({0.0, 0.0});
      auto aruco_fovea_vec = aruco_fovea_in.read(),
           aruco_wide_vec = aruco_wide_in.read();
      while (!global::flag_term) {
        // Update wide aruco info string
        if (aruco_wide_in.next(aruco_wide_vec, false) &&
            aruco_wide_vec->size() > 0) {
          auto &aruco_wide = aruco_wide_vec->at(0);
          info_wide = "";
          for (const auto &p : aruco_wide.corners)
            info_wide += std::to_string(p.x) + "," + std::to_string(p.y) + " ";
        }
        // Check if position is available
        aruco_fovea_in.next(aruco_fovea_vec, true);
        if (aruco_fovea_vec->size() == 0)
          continue;
        if (!enable)
          continue;
        const auto &aruco_fovea = aruco_fovea_vec->at(0);
        // Get current mems position
        double Vx = aruco_fovea.volt.x, Vy = aruco_fovea.volt.y;
        Vx += kp * aruco_fovea.pos.x;
        Vy -= kp * aruco_fovea.pos.y;
        normalize(Vx);
        normalize(Vy);
        // Send to mems
        ctx.mems_pos.flush().write({Vx, Vy});
        // Read ArUco info from wide camera
        auto aruco_wide_vec = aruco_wide_in.read();
        if (aruco_wide_vec == nullptr || aruco_wide_vec->size() == 0)
          continue;
        // Check if the marker is the same
        if (prev_id != 0 && aruco_fovea.id == 0) {
          std::stringstream ss;
          ss << "id = " << prev_id << " ; ";
          ss << "volt = " << info_volt << " ; ";
          ss << "fovea = " << info_fovea << " ; ";
          ss << "wide = " << info_wide << " ;";
          std::cout << ss.str() << std::endl;
          if (out == nullptr) {
            out = new outfile::Item("data.txt");
            outfile::items.push_back(out);
          }
          *out->fs << ss.str() << std::endl;
        }
        info_fovea = "";
        for (const auto &p : aruco_fovea.corners)
          info_fovea += std::to_string(p.x) + "," + std::to_string(p.y) + " ";
        info_volt = std::to_string(aruco_fovea.volt.x) + "," +
                    std::to_string(aruco_fovea.volt.y);
        prev_id = aruco_fovea.id;
      };
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME);
    ctx.close();
    aruco_wide_in.close();
    aruco_fovea_in.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:aruco:ArUco-locator] "

cv::Point2d center(const std::vector<cv::Point2f> &corners,
                   const cv::Mat &frame) {
  cv::Point2d center(0, 0);
  for (const auto &p : corners) {
    center.x += p.x;
    center.y += p.y;
  }
  center.x = center.x / corners.size() - frame.cols / 2.0;
  center.y = center.y / corners.size() - frame.rows / 2.0;
  return center;
}

cv::Mat find_aruco(ArUcoPipe &aruco_out, const cv::Mat &frame,
                   cv::Point2d &volt, bool transform,
                   bool view_transform = false) {
  // Process new frame
  cv::Mat mat, global;
  cv::cvtColor(frame, mat, cv::COLOR_RGBA2GRAY);
  if (transform || view_transform) {
    // Resize to 1/4
    cv::resize(mat, mat, cv::Size(), 0.25, 0.25);
    // extend to float 32
    mat.convertTo(mat, CV_32FC1);
    cv::Mat global;
    cv::GaussianBlur(mat, mat, cv::Size(3, 3), 0, 0);
    cv::GaussianBlur(mat, global, cv::Size(99, 99), 0, 0);
    // Run threshold according to global light
    cv::subtract(mat, global, mat);
    cv::threshold(mat, mat, 0, 255, cv::THRESH_BINARY);
    mat.convertTo(mat, CV_8UC1);
    // cv::equalizeHist(mat, mat);
  }
  cv::Mat view;
  if (view_transform) {
    cv::resize(mat, view, cv::Size(), 4, 4);
    cv::cvtColor(view, view, cv::COLOR_GRAY2RGBA);
  } else {
    view = frame.clone();
  }
  // The list of all detected markers
  std::vector<ArUcoInfo> info;
  // Do the detection
  std::vector<int> ids;
  std::vector<std::vector<cv::Point2f>> corners;
  auto const dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_100);
  auto const dictionaryPtr = cv::makePtr<cv::aruco::Dictionary>(dictionary);
  cv::aruco::detectMarkers(mat, dictionaryPtr, corners, ids);
  // if at least one marker detected
  for (unsigned i = 0; i < ids.size(); i++) {
    const int id = ids[i];
    const auto c = corners[i];
    auto rect = std::vector<cv::Point2f>(c.begin(), c.end());
    if (transform) {
      for (auto &p : rect) {
        p.x *= 4;
        p.y *= 4;
      }
    }
    ArUcoInfo marker_info = {
        .id = id, .pos = center(rect, frame), .corners = rect, .volt = volt};
    info.push_back(marker_info);
  }
  aruco_out.write(info);
  return view;
}

#undef LOGNAME
#define LOGNAME "[task:aruco:locator-wide] "

std::thread aruco_locator(MatPipe &mat_in, ArUcoPipe &aruco_out,
                          MatPipe &view_out, bool &transform,
                          bool &view_transform) {
  return std::thread([&]() {
    try {
      auto mat = mat_in.read();
      auto volt = cv::Point2d(0, 0);
      while (!global::flag_term) {
        mat_in.next(mat, true);
        auto view =
            find_aruco(aruco_out, *mat, volt, transform, view_transform);
        view_out.write(view);
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    mat_in.close();
    aruco_out.close();
    view_out.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:aruco:renderer-wide] "

std::thread wide_renderer(Tile &tile, ArUcoPipe &aruco_in, MatPipe &view_in) {
  return std::thread([&]() {
    std::vector<ArUcoInfo> aruco_list;
    try {
      tile.auto_raster = false;
      tile.text("WIDE | ID = < > | Px = -- | Py = --");
      auto frame = view_in.read();
      auto aruco_vec = aruco_in.read();
      while (!global::flag_term) {
        view_in.next(frame, true);
        aruco_in.next(aruco_vec, false);
        if (aruco_vec != nullptr && aruco_vec->size() > 0) {
          auto &aruco = aruco_vec->at(0);
          // Update tile text
          std::stringstream ss;
          ss << "WIDE | ID = <" << aruco.id << ">"
             << " | Px = " << fmt(aruco.pos.x, 2, 2)
             << " | Py = " << fmt(aruco.pos.y, 2, 2);
          tile.text(ss.str());
          // Update ArUco list
          if (aruco_list.size() == 0) {
            aruco_list.push_back(aruco);
          } else if (aruco_list.back().id == aruco.id ||
                     aruco_list.back().id == 0) {
            aruco_list.back() = aruco;
          } else {
            aruco_list.push_back(aruco);
          }
        }
        for (auto &aruco : aruco_list) {
          cv::Point pos = aruco.pos;
          pos.x += frame->cols / 2;
          pos.y += frame->rows / 2;
          if (aruco.id == 0) {
            // Draw red cross
            cv::drawMarker(*frame, pos, color::red(), cv::MARKER_TILTED_CROSS,
                           80, 4);
          } else {
            // Draw green circle with ID number on bottom
            cv::drawMarker(*frame, pos, color::blue(), cv::MARKER_CROSS, 20,
                           10);
            pos.x += 20;
            pos.y += 20;
            cv::putText(*frame, std::to_string(aruco.id), pos, 0, 1,
                        color::blue(), 2);
          }
        }
        tile.use(*frame);
        tile.raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    aruco_in.close();
    view_in.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:aruco:locator-fovea] "

std::thread aruco_locator(FoveaPipe &fovea_in, ArUcoPipe &aruco_out,
                          FoveaPipe &view_out, bool &transform,
                          bool &view_transform) {
  return std::thread([&]() {
    try {
      auto fovea = fovea_in.read();
      while (!global::flag_term) {
        fovea_in.next(fovea, true);
        auto volt = fovea->volt();
        auto view =
            find_aruco(aruco_out, fovea->mat, volt, transform, view_transform);
        view_out.write({
            .tag = fovea->tag,
            .x = fovea->x,
            .y = fovea->y,
            .mat = view,
        });
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    fovea_in.close();
    aruco_out.close();
    view_out.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:aruco:renderer-fovea] "

std::thread fovea_renderer(Tile &tile, ArUcoPipe &aruco_in,
                           FoveaPipe &view_in) {
  return std::thread([&]() {
    try {
      tile.auto_raster = false;
      auto fovea = view_in.read();
      auto aruco_vec = aruco_in.read();
      while (!global::flag_term) {
        view_in.next(fovea, true);
        aruco_in.next(aruco_vec, false);
        if (aruco_vec != nullptr) {
          if (aruco_vec->size() == 0) {
            std::stringstream ss;
            ss << "FOVEA | ID = < >"
               << " | Vx = " << fmt(fovea->x, 2, 2)
               << " | Vy = " << fmt(fovea->y, 2, 2);
            tile.text(ss.str());
          } else {
            auto &aruco = aruco_vec->at(0);
            cv::Point pos = aruco.pos;
            pos.x += fovea->mat.cols / 2;
            pos.y += fovea->mat.rows / 2;
            cv::drawMarker(fovea->mat, pos, color::red(), cv::MARKER_CROSS, 80,
                           8);
            std::stringstream ss;
            ss << "FOVEA | ID = <" << aruco.id << ">"
               << " | Vx = " << fmt(aruco.volt.x, 2, 2)
               << " | Vy = " << fmt(aruco.volt.y, 2, 2);
            tile.text(ss.str());
          }
        }
        tile.use(fovea->mat);
        tile.raster();
      }
    }
    EXPECT_END_OF_STREAM
    CATCH_ASSERT(LOGNAME)
    aruco_in.close();
    view_in.close();
    std::cerr << LOGNAME " terminated." << std::endl;
  });
}

#undef LOGNAME
#define LOGNAME "[task:aruco] "

void tasks::aruco(Context &ctx) {
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
  Tile recenter_btn(cv::Rect{x, y, btn_w, btn_h}, pad);
  x += btn_w;
  Tile kp_slider(cv::Rect{x, y, 2 * btn_w, btn_h}, pad);
  kp_slider.val.x = kp * 25;
  kp_slider.text("kp = " + fmt(kp * 100, 1, 2, ' ', false) + '%');
  bool t_wide = false, vt_wide = false, t_fovea = true, vt_fovea = false,
       pid_enable = true;
  std::vector<Tile *> tiles = {
      &wide_tile.use([&](Tile &tile, bool state_change) {
        if (state_change)
          vt_wide = tile.is_active();
      }),
      &fovea_tile.use([&](Tile &tile, bool state_change) {
        if (state_change)
          vt_fovea = tile.is_active();
      }),
      &back_btn,
      &recenter_btn.as(TileMode::BUTTON)
           .text("RECENTER")
           .use([&ctx, &pid_enable](Tile &tile, bool) {
             if (pid_enable) {
               pid_enable = false;
               std::this_thread::sleep_for(std::chrono::milliseconds(100));
               ctx.mems_pos.flush().write({0.0, 0.0});
               std::this_thread::sleep_for(std::chrono::milliseconds(100));
               pid_enable = true;
             }
           }),
      &kp_slider.as(TileMode::X_SLIDER).use([&](Tile &tile, bool) {
        kp = clamp(tile.val.x, 0.0, 1.0) / 25;
        kp_slider.text("kp = " + fmt(kp * 100, 2, 4, ' ', false) + '%');
      }),
  };
  // Launch worker threads
  std::vector<global::ThreadInfo> threads;
  MatPipe wide_pipe;
  FoveaPipe fovea_pipe;
  ArUcoPipe aruco_wide_pipe, aruco_fovea_pipe;
  threads.push_back({
      "aruco-locator/wide",
      aruco_locator(ctx.cap_wide, aruco_wide_pipe, wide_pipe, t_wide, vt_wide),
  });
  threads.push_back({
      "renderer/wide",
      wide_renderer(wide_tile, aruco_wide_pipe, wide_pipe),
  });
  threads.push_back({
      "aruco-locator/fovea",
      aruco_locator(ctx.cap_fovea, aruco_fovea_pipe, fovea_pipe, t_fovea,
                    vt_fovea),
  });
  threads.push_back({
      "renderer/fovea",
      fovea_renderer(fovea_tile, aruco_fovea_pipe, fovea_pipe),
  });
  threads.push_back({
      "pid-controller",
      pid_controller(ctx, aruco_wide_pipe, aruco_fovea_pipe, pid_enable),
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
  aruco_wide_pipe.close();
  aruco_fovea_pipe.close();
  for (auto &el : threads) {
    std::cerr << LOGNAME "waiting for " << el.name << std::endl;
    el.thread.join();
  }
  std::cerr << LOGNAME "terminated." << std::endl;
}
