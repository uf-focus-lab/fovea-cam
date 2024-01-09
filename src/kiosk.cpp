#include <cmath>
#include <glob.h>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "graphics/canvas.h"
#include "graphics/x11.h"

#include "splash.png.h"

#define LOG_NAME "[kiosk] "

static const unsigned int PAD = 32;

double clamp(double min, double max, double val) {
  if (val < min)
    return min;
  else if (val > max)
    return max;
  else
    return val;
}

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

class Tile {
public:
  bool active = false;
  double value = 0;
  cv::Rect bbox, content;
  cv::Mat mat;
  unsigned int pad;
  Tile(cv::Rect bbox, unsigned int pad = PAD) : bbox(bbox), pad(pad) {
    content = cv::Rect(bbox.x + pad, bbox.y + pad, bbox.width - 2 * pad,
                       bbox.height - 2 * pad);
    mat = cv::Mat(bbox.height, bbox.width, CV_8UC4, cv::Scalar(0, 0, 0, 0));
  }
  bool contains(PointerEvent pos) {
    return pos.x >= bbox.x && pos.x < bbox.x + bbox.width && pos.y >= bbox.y &&
           pos.y < bbox.y + bbox.height;
  }
  double pct_x(int x) {
    return (double)(x - content.x) / (double)content.width;
  }
  double pct_y(int y) {
    return (double)(y - content.y) / (double)content.height;
  }
  Tile &fill(cv::Scalar color, double x1, double x2, double y1, double y2) {
    const unsigned x = rint(x1 * content.width) + pad,
                   y = rint(y1 * content.height) + pad,
                   w = rint((x2 - x1) * content.width),
                   h = rint((y2 - y1) * content.height);
    cv::rectangle(mat, cv::Rect(x, y, w, h), color, cv::FILLED);
    return *this;
  }
  Tile &fill(cv::Scalar color, double x2 = 1, double y2 = 1) {
    return fill(color, 0, x2, 0, y2);
  }
  Tile &text(std::string text, cv::Scalar color, double pad = 0.3) {
    try {
      const unsigned height = content.height;
      // Center position
      const int fs = content.height / 32, ft = fs * 1.2;
      const cv::Size t_canvas_size =
          cv::getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fs, ft, nullptr);
      // 2p = 1 - h / (h + 2x) => x = (1 - 1/(1 -2p)) - h) / 2
      const int fp = rint(double(t_canvas_size.height) * pad / (1 - 2 * pad));
      const cv::Size t_bleed_size =
          cv::Size(t_canvas_size.width + fp * 2, t_canvas_size.height + fp * 2);
      // Create canvas for text mask
      cv::Mat mask(t_bleed_size, CV_8UC1, cv::Scalar(0));
      cv::putText(mask, text, cv::Point(fp, t_bleed_size.height - fp),
                  cv::FONT_HERSHEY_DUPLEX, fs, cv::Scalar(255), ft);
      // Resize to fit
      const double scale = (double)height / (double)t_bleed_size.height;
      cv::resize(mask, mask, cv::Size(), scale, scale, cv::INTER_AREA);
      const cv::Size size = mask.size();
      const cv::Rect roi =
          cv::Rect((bbox.width - size.width) / 2,
                   (bbox.height - size.height) / 2, size.width, size.height);
      // Convert to colored mask
      cv::Mat text_mat(size, CV_8UC4, color);
      // Apply text mat with given color
      cv::copyTo(text_mat, mat(roi), mask);
    } catch (cv::Exception &e) {
      std::cerr << LOG_NAME "Failed to render text: " << e.what() << std::endl;
    }
    return *this;
  }
  bool handle(PointerEvent pos) {
    const bool ctn = contains(pos);
    if (pos.button) {
      if (ctn) {
        active = true;
        value = clamp(0, 1, pct_x(pos.x));
      } else {
        active = false;
      }
    } else if (active && ctn) {
      value = clamp(0, 1, pct_x(pos.x));
    }
    return active;
  }
};

int contains(cv::Rect rect, PointerEvent pos) {
  return pos.x >= rect.x && pos.x < rect.x + rect.width && pos.y >= rect.y &&
         pos.y < rect.y + rect.height;
}

extern char **environ;

std::string EXP(double v) { return str(v * 20); }

std::string FPS(double v) { return v > 0.1 ? str(v * 110 - 10.5) : "N/A"; }

int run(graphics::X11FB &fb, const char *_argv[], double exp, double fps,
        std::string cmd) {
  std::vector<const char *> argv;
  if (exp < 0.01)
    exp = 0.01;
  std::cout << "EXP=" << EXP(exp) << " ";
  if (fps > 0.1)
    std::cout << "FPS=" << FPS(fps) << " ";
  std::cout << _argv[0] << " " << cmd;
  return 0;
}

int kiosk(const char *argv[]) {
  std::cerr << LOG_NAME "Entering Kiosk mode" << std::endl;

  if (graphics::x11env()) {
    std::cerr << LOG_NAME "Failed to set DISPLAY environment." << std::endl;
    return 1;
  }

  graphics::X11FB fb;
  if (!fb.isOpen()) {
    std::cerr << LOG_NAME "Failed to open X11 framebuffer." << std::endl;
    return 1;
  }

  graphics::Canvas canvas(fb.shape().w, fb.shape().h);
  const cv::Mat splash(SPLASH_PNG_H, SPLASH_PNG_W, CV_8UC4,
                       (char *)SPLASH_PNG_DATA);
  canvas.clear().show(splash).apply(fb.buffer());
  fb.sync();
  // Prepare tiles for interaction
  const unsigned w = fb.shape().w, h = fb.shape().h / 8,
                 pad = rint(double(h) / 8.f);
  const auto bg = cv::Scalar(32, 32, 32, 255),
             fg = cv::Scalar(128, 64, 32, 255),
             stroke = cv::Scalar(128, 192, 64, 255);
  // Tile for EXP slider
  unsigned y = fb.shape().h * 5 / 8;
  auto exp = Tile(cv::Rect(0, y, w, h), pad);
  exp.value = 1.0 / 10.0;
  exp.fill(bg).fill(fg, exp.value).text(str("EXP = ", exp.value * 10), stroke);
  // Tile for FPS slider
  y += h;
  auto fps = Tile(cv::Rect(0, y, w, h), pad);
  fps.value = 0;
  fps.fill(bg).fill(fg, exp.value).text("FPS = ---", stroke);
  // Tile for action buttons
  y += h;
  auto btn_move =
           Tile(cv::Rect(0, y, w / 4, h), pad).fill(bg).text("MOV", stroke),
       btn_track =
           Tile(cv::Rect(w / 4, y, w / 4, h), pad).fill(bg).text("TRK", stroke),
       btn_match = Tile(cv::Rect(2 * w / 4, y, w / 4, h), pad)
                       .fill(bg)
                       .text("MCH", stroke),
       btn_rec = Tile(cv::Rect(3 * w / 4, y, w / 4, h), pad)
                     .fill(bg)
                     .text("CAP", stroke);

  std::cerr << LOG_NAME "Start interaction" << std::endl;
  canvas.show(exp.mat, exp.bbox)
      .show(fps.mat, fps.bbox)
      .show(btn_move.mat, btn_move.bbox)
      .show(btn_track.mat, btn_track.bbox)
      .show(btn_match.mat, btn_match.bbox)
      .show(btn_rec.mat, btn_rec.bbox)
      .apply(fb.buffer());
  // Enter event loop
  while (1) {
    fb.sync();
    PointerEvent pos = fb.wait_pointer();
    // Check for corresponding tile
    if (exp.handle(pos)) {
      exp.fill(bg).fill(fg, exp.value).text("EXP = " + EXP(exp.value), stroke);
      canvas.show(exp.mat, exp.bbox).apply(fb.buffer());
      fb.sync();
    }
    if (fps.handle(pos)) {
      std::string value = "FPS = " + FPS(fps.value);
      fps.fill(bg).fill(fg, fps.value).text(value, stroke);
      canvas.show(fps.mat, fps.bbox).apply(fb.buffer());
      fb.sync();
    }
    if (btn_move.handle(pos)) {
      btn_move.fill(bg).fill(stroke).text("MOV", fg);
      canvas.clear()
          .show(splash)
          .show(btn_move.mat, btn_move.bbox)
          .show(btn_track.mat, btn_track.bbox)
          .show(btn_match.mat, btn_match.bbox)
          .show(btn_rec.mat, btn_rec.bbox)
          .apply(fb.buffer());
      fb.sync();
      return run(fb, argv, exp.value, fps.value, "move");
    }
    if (btn_track.handle(pos)) {
      btn_track.fill(bg).fill(stroke).text("TRK", fg);
      canvas.clear()
          .show(splash)
          .show(btn_move.mat, btn_move.bbox)
          .show(btn_track.mat, btn_track.bbox)
          .show(btn_match.mat, btn_match.bbox)
          .show(btn_rec.mat, btn_rec.bbox)
          .apply(fb.buffer());
      fb.sync();
      return run(fb, argv, exp.value, fps.value, "track");
    }
    if (btn_match.handle(pos)) {
      btn_match.fill(bg).fill(stroke).text("MCH", fg);
      canvas.clear()
          .show(splash)
          .show(btn_move.mat, btn_move.bbox)
          .show(btn_track.mat, btn_track.bbox)
          .show(btn_match.mat, btn_match.bbox)
          .show(btn_rec.mat, btn_rec.bbox)
          .apply(fb.buffer());
      fb.sync();
      return run(fb, argv, exp.value, fps.value, "match");
    }
    if (btn_rec.handle(pos)) {
      btn_rec.fill(bg).fill(stroke).text("CAP", fg);
      canvas.clear()
          .show(splash)
          .show(btn_move.mat, btn_move.bbox)
          .show(btn_track.mat, btn_track.bbox)
          .show(btn_match.mat, btn_match.bbox)
          .show(btn_rec.mat, btn_rec.bbox)
          .apply(fb.buffer());
      fb.sync();
      return run(fb, argv, exp.value, fps.value, "capture");
    }
  }
  return 0;
}
