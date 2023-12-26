#include <cmath>
#include <cstring>
#include <glob.h>
#include <iostream>
#include <opencv2/core/types.hpp>
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
  Tile &text(std::string text, cv::Scalar color, double scale = 3) {
    // Center position
    const cv::Size size = cv::getTextSize(text, cv::FONT_HERSHEY_DUPLEX, scale,
                                          rint(scale * 1.6), nullptr);
    const unsigned x = (bbox.width - size.width) / 2,
                   y = (bbox.height + size.height) / 2;
    cv::putText(mat, text, cv::Point(x, y), cv::FONT_HERSHEY_DUPLEX, scale,
                color, 2);
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

int run(graphics::X11FB &fb, const char *_argv[], double exp, double fps,
        std::string cmd) {
  std::vector<const char *> argv;
  argv.push_back(_argv[0]);
  argv.push_back(cmd.c_str());
  const auto EXP = std::to_string(exp < 0.01 ? 0.01 : exp);
  setenv("EXP", EXP.c_str(), 1);
  if (fps > 0.1)
    setenv("FPS", std::to_string((int)rint(fps * 110 - 10.5)).c_str(), 1);
  // Fork and exec
  int pid;
  if ((pid = fork()) == 0) {
    std::cerr << LOG_NAME "Forked child process " << argv[0] << ' ' << argv[1]
              << std::endl;
    execvp(argv[0], (char *const *)argv.data());
    std::cerr << LOG_NAME "Failed to exec child process" << std::endl;
    exit(1);
  }
  fb.flush();
  std::cerr << LOG_NAME "Waiting for child" << std::endl;
  // Wait for child process to terminate
  while (1) {
    // Check if child process is still running
    if (waitpid(pid, NULL, WNOHANG) == pid)
      break;
    // Check for input event
    PointerEvent pos;
    while ((pos = fb.wait_pointer()).valid) {
      if (pos.button) {
        kill(pid, SIGINT);
      }
    }
  }
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
  const unsigned w = fb.shape().w, h = fb.shape().h / 8;
  const auto bg = cv::Scalar(32, 32, 32, 255),
             fg = cv::Scalar(128, 64, 32, 255),
             stroke = cv::Scalar(128, 192, 64, 255);
  // Tile for EXP slider
  unsigned y = fb.shape().h * 5 / 8;
  auto exp = Tile(cv::Rect(0, y, w, h));
  exp.value = 1.0 / 10.0;
  exp.fill(bg).fill(fg, exp.value).text("EXP = 1.0", stroke);
  // Tile for FPS slider
  y += h;
  auto fps = Tile(cv::Rect(0, y, w, h));
  fps.value = 0;
  fps.fill(bg).fill(fg, exp.value).text("FPS = ---", stroke);
  // Tile for action buttons
  y += h;
  auto btn_move = Tile(cv::Rect(0, y, w / 4, h)).fill(bg).text("MOV", stroke),
       btn_track =
           Tile(cv::Rect(w / 4, y, w / 4, h)).fill(bg).text("TRK", stroke),
       btn_match =
           Tile(cv::Rect(2 * w / 4, y, w / 4, h)).fill(bg).text("MCH", stroke),
       btn_rec =
           Tile(cv::Rect(3 * w / 4, y, w / 4, h)).fill(bg).text("CAP", stroke);

  std::cerr << LOG_NAME "Start interaction" << std::endl;
  canvas.show(exp.mat, exp.bbox)
      .show(fps.mat, fps.bbox)
      .show(btn_move.mat, btn_move.bbox)
      .show(btn_track.mat, btn_track.bbox)
      .show(btn_match.mat, btn_match.bbox)
      .show(btn_rec.mat, btn_rec.bbox)
      .apply(fb.buffer());
  fb.sync();
  // Enter event loop
  while (1) {
    PointerEvent pos = fb.wait_pointer();
    // Check for corresponding tile
    if (exp.handle(pos)) {
      std::string value = std::to_string(exp.value * 10);
      exp.fill(bg).fill(fg, exp.value).text("EXP = " + value, stroke);
      canvas.show(exp.mat, exp.bbox).apply(fb.buffer());
      fb.sync();
    }
    if (fps.handle(pos)) {
      std::string value =
          fps.value > 0.1 ? std::to_string((int)rint(fps.value * 110 - 10.5))
                          : "N/A";
      fps.fill(bg).fill(fg, fps.value).text("FPS = " + value, stroke);
      canvas.show(fps.mat, fps.bbox).apply(fb.buffer());
      fb.sync();
    }
    if (btn_move.handle(pos)) {
      btn_move.fill(bg).fill(stroke).text("MOV", fg);
      canvas.show(btn_move.mat, btn_move.bbox).apply(fb.buffer());
      fb.sync();
      run(fb, argv, exp.value, fps.value, "move");
      canvas.apply(fb.buffer());
      fb.sync();
    }
    if (btn_track.handle(pos)) {
      btn_track.fill(bg).fill(stroke).text("TRK", fg);
      canvas.show(btn_track.mat, btn_track.bbox).apply(fb.buffer());
      fb.sync();
      run(fb, argv, exp.value, fps.value, "track");
      canvas.apply(fb.buffer());
      fb.sync();
    }
    if (btn_match.handle(pos)) {
      btn_match.fill(bg).fill(stroke).text("MCH", fg);
      canvas.show(btn_match.mat, btn_match.bbox).apply(fb.buffer());
      fb.sync();
      run(fb, argv, exp.value, fps.value, "match");
      canvas.apply(fb.buffer());
      fb.sync();
    }
    if (btn_rec.handle(pos)) {
      btn_rec.fill(bg).fill(stroke).text("CAP", fg);
      canvas.show(btn_rec.mat, btn_rec.bbox).apply(fb.buffer());
      fb.sync();
      run(fb, argv, exp.value, fps.value, "capture");
      canvas.apply(fb.buffer());
      fb.sync();
    }
  }
  return 0;
}
