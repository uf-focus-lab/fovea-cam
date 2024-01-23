#include "canvas.h"
#include "alpha.h"
#include "graphics/X11.h"
#include "shared.h"
#include "util/time.h"

#include <cstring>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <unistd.h>

cv::Mat transform_mat(const cv::Mat &src, const int &transform) {
  cv::Mat mat(src);
  // Use pre-defined rotations
  if (transform == graphics::transform::ROTATE_90) {
    cv::rotate(src, mat, cv::RotateFlags::ROTATE_90_CLOCKWISE);
  } else if (transform == graphics::transform::ROTATE_180) {
    cv::rotate(src, mat, cv::RotateFlags::ROTATE_180);
  } else if (transform == graphics::transform::ROTATE_270) {
    cv::rotate(src, mat, cv::RotateFlags::ROTATE_90_COUNTERCLOCKWISE);
  } else {
    if (transform & graphics::transform::TRANSPOSE) {
      cv::transpose(src, mat);
    }
    const auto flip = transform & graphics::transform::FLIP_XY;
    if (flip == graphics::transform::FLIP_XY)
      cv::flip(src, mat, -1);
    else if (flip & graphics::transform::FLIP_X)
      cv::flip(src, mat, 0);
    else if (flip & graphics::transform::FLIP_Y)
      cv::flip(src, mat, 1);
  }
  return mat;
}

cv::Point transform_point(cv::Point p, cv::Size s, const int &transform) {
  if (transform & graphics::transform::TRANSPOSE) {
    p = {p.y, p.x};
    s = {s.height, s.width};
  }
  if (transform & graphics::transform::FLIP_X)
    p.x = s.width - p.x - 1;
  if (transform & graphics::transform::FLIP_Y)
    p.y = s.height - p.y - 1;
  return p;
}

void merge_channels(cv::Mat &a, cv::Mat &b) {
  std::vector<cv::Mat> channels_a, channels_b;
  cv::split(a, channels_a);
  cv::split(b, channels_b);
  for (auto ch : channels_b) {
    channels_a.push_back(ch);
  }
  std::cout << channels_a.size() << std::endl;
  cv::merge(channels_a, a);
}

namespace graphics {

void Canvas::cursor_init(int size) {
  cursor_size = size;
  size *= 8;
  const cv::Size s(size, size);
  cursor_up = cv::Mat(s, CV_8UC4, color::mono(0, 0));
  cursor_down = cv::Mat(s, CV_8UC4, color::mono(0, 0));
  const int t = size / 16, r = size / 2 - t;
  const cv::Point c(size / 2, size / 2);
  // Draw hiDPI circle and then scale down
  cv::circle(cursor_up, c, t, color::mono(1, 0.75), cv::FILLED);
  cv::circle(cursor_up, c, t + t / 2, color::mono(0.5, 0.75), t);
  cv::circle(cursor_up, c, 2 * t + t / 2, color::mono(0, 0.5), t);
  cv::circle(cursor_down, c, r, color::mono(0, 0.25), cv::FILLED);
  cv::circle(cursor_down, c, t, color::mono(1, 0.75), cv::FILLED);
  cv::circle(cursor_down, c, r, color::mono(0, 0.25), t);
  cv::circle(cursor_down, c, r - t / 2, color::mono(1, 0.75), t);
  // Scale down
  const auto s_target = cv::Size(cursor_size, cursor_size);
  cv::resize(cursor_up, cursor_up, s_target, 0, 0, cv::INTER_AREA);
  cv::resize(cursor_down, cursor_down, s_target, 0, 0, cv::INTER_AREA);
}

cv::Mat Canvas::handle_pointer(const PointerEvent *e) {
  if (e != nullptr) {
    cv::Mat disp;
    const cv::Rect dst(e->x, e->y, cursor_size, cursor_size);
    const int bleed = cursor_size / 2;
    cv::copyMakeBorder(mat, disp, bleed, bleed, bleed, bleed,
                       cv::BORDER_CONSTANT, color::mono(0, 255));
    auto cursor = disp(dst).clone();
    alpha_blend(cursor, e->is_down(1) ? cursor_down : cursor_up);
    cursor.copyTo(disp(dst));
    return disp(cv::Rect(bleed, bleed, width, height)).clone();
  } else {
    return mat;
  }
}

Canvas::Canvas(cv::Size size) : mat(size.height, size.width, CV_8UC4) {
  this->width = size.width;
  this->height = size.height;
  cursor_init(std::min(width, height) / 8);
}

Canvas::Canvas(cv::Size size, unsigned line_length)
    : mat(size.height, line_length, CV_8UC4) {
  this->width = size.width;
  this->height = size.height;
  cursor_init(std::min(width, height) / 8);
}

Canvas::Canvas(unsigned width, unsigned height) : mat(height, width, CV_8UC4) {
  this->width = width;
  this->height = height;
  cursor_init(std::min(width, height) / 8);
}

Canvas::Canvas(unsigned width, unsigned height, unsigned line_length)
    : mat(height, line_length, CV_8UC4) {
  this->width = width;
  this->height = height;
  cursor_init(std::min(width, height) / 8);
}

Canvas::~Canvas() {
  if (pe != nullptr) {
    delete pe;
    pe = nullptr;
  }
}

int Canvas::get_transform() { return transform; };
int Canvas::set_transform(int transform) {
  const int diff = transform ^ this->transform;
  if (diff)
    transform_mat(mat, diff);
  this->transform = transform;
  return transform;
};

cv::Mat Canvas::Mat() { return mat; };

cv::Size Canvas::shape() {
  if (transform & transform::TRANSPOSE) {
    return {static_cast<int>(height), static_cast<int>(width)};
  } else {
    return {static_cast<int>(width), static_cast<int>(height)};
  }
}

Canvas &Canvas::clear() {
  clear(0);
  return *this;
}

Canvas &Canvas::clear(uint8_t color) {
  mat = color;
  return *this;
}

Canvas &Canvas::clear(cv::Scalar color) {
  mat = color;
  return *this;
}

// Send the current buffer to display
Canvas &Canvas::show() {
  show(mat, transform);
  return *this;
}

Canvas &Canvas::show(const cv::Mat &src, int transform) {
  show(src, cv::Rect{0, 0, shape().width, shape().height}, transform);
  return *this;
}

Canvas &Canvas::show(const cv::Mat &src, cv::Rect tile, int transform) {
  cv::Mat dst;
  // Scale down to fit, retain aspect ratio
  if (src.cols > tile.width || src.rows > tile.height) {
    const auto ratio = std::min(tile.width / static_cast<double>(src.cols),
                                tile.height / static_cast<double>(src.rows));
    cv::resize(src, dst, cv::Size(), ratio, ratio);
  } else {
    dst = src;
  }
  // Place to center of tile
  cv::Point pos(tile.x + (tile.width - dst.cols) / 2,
                tile.y + (tile.height - dst.rows) / 2);
  render(dst, pos, transform);
  return *this;
}

Canvas &Canvas::show(Tile &tile, int transform) {
  auto readout = tile.read();
  if (readout != nullptr)
    render(readout->mat, {readout->bbox.x, readout->bbox.y}, transform);
  return *this;
}

Canvas &Canvas::show(std::vector<Tile *> &tiles, int transform) {
  for (auto tile : tiles)
    show(*tile, transform);
  return *this;
}

Canvas &Canvas::render(const cv::Mat &src, cv::Point pos, int transform) {
  try { // Apply transform
    const auto tile_transform = transform ^ this->transform;
    auto dst = transform_mat(src, tile_transform);
    auto corner_pos = transform_point(pos, shape(), tile_transform),
         corner_off =
             transform_point({0, 0}, {src.cols, src.rows}, tile_transform);
    pos = corner_pos - corner_off;
    // Check if trim is necessary
    cv::Rect trim = {0, 0, dst.cols, dst.rows};
    if (dst.cols + pos.x > static_cast<int>(width)) {
      trim.width = width - pos.x;
    }
    if (dst.rows + pos.y > static_cast<int>(height)) {
      trim.height = height - pos.y;
    }
    dst = dst(trim);
    // Place the display image to buffer
    dst.copyTo(mat(cv::Rect(pos, dst.size())));
  } catch (std::exception &e) {
    std::cerr << "[graphics:Canvas] render(): " << e.what() << std::endl;
  }
  return *this;
}

Canvas &Canvas::apply(void *fb, const PointerEvent *event) {
  cv::Mat disp;
  static unsigned long last_valid_pe;
  if (event != nullptr && event->valid) {
    last_valid_pe = Time::ms();
    if (pe != nullptr) {
      delete pe;
    }
    pe = new PointerEvent(*event);
    disp = handle_pointer(event);
  } else if (pe != nullptr) {
    if (Time::ms() - last_valid_pe > 500) {
      delete pe;
      pe = nullptr;
    } else {
      disp = handle_pointer(pe);
    }
  } else {
    disp = mat;
  }
  const auto s = disp.size();
  memcpy(fb, disp.data, s.height * s.width * 4);
  return *this;
}

Canvas &Canvas::apply(X11FB &fb, const PointerEvent *event) {
  apply(fb.buffer(), event);
  fb.sync();
  return *this;
}

} // namespace graphics
