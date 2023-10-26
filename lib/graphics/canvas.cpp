#include "canvas.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <type_traits>
#include <unistd.h>

namespace canvas {

cv::Mat transform_mat(const cv::Mat &src, const int &transform) {
  cv::Mat mat(src);
  // Use pre-defined rotations
  if (transform == transform::ROTATE_90) {
    cv::rotate(src, mat, cv::RotateFlags::ROTATE_90_CLOCKWISE);
  } else if (transform == transform::ROTATE_180) {
    cv::rotate(src, mat, cv::RotateFlags::ROTATE_180);
  } else if (transform == transform::ROTATE_270) {
    cv::rotate(src, mat, cv::RotateFlags::ROTATE_90_COUNTERCLOCKWISE);
  } else {
    if (transform & transform::TRANSPOSE) {
      cv::transpose(src, mat);
    }
    const auto flip = transform & FLIP_XY;
    if (flip == transform::FLIP_XY)
      cv::flip(src, mat, -1);
    else if (flip & transform::FLIP_X)
      cv::flip(src, mat, 0);
    else if (flip & transform::FLIP_Y)
      cv::flip(src, mat, 1);
  }
  return mat;
}

const auto _fork = fork;

cv::Point transform_point(cv::Point p, shape s, const int &transform) {
  if (transform & transform::TRANSPOSE) {
    p = {p.y, p.x};
    s = {s.h, s.w};
  }
  if (transform & transform::FLIP_X)
    p.x = s.w - p.x - 1;
  if (transform & transform::FLIP_Y)
    p.y = s.h - p.y - 1;
  return p;
}

Canvas::Canvas(std::string path, int transform) : fb(path) {
  fb_info = fb.info();
  set_transform(transform);
  mat = cv::Mat(shape().h, shape().w, CV_8UC4);
}

int Canvas::get_transform() { return transform; };
int Canvas::set_transform(int transform) {
  const int diff = transform ^ this->transform;
  if (diff)
    transform_mat(mat, diff);
  this->transform = transform;
  return transform;
};

int Canvas::fork() {
  pid_t pid = _fork();
  if (pid == 0) {
    // Child process
    fb.reopen();
  }
  return pid;
}

cv::Mat Canvas::Mat() { return mat; };

canvas::shape Canvas::shape() {
  const auto info = fb_info;
  if (transform & transform::TRANSPOSE) {
    return {.w = info.height, .h = info.width};
  } else {
    return {.w = info.width, .h = info.height};
  }
}

void Canvas::clear() { clear(0); }

void Canvas::clear(uint8_t color) {
  mat = color;
  show();
}

void Canvas::clear(cv::Scalar color) {
  mat = color;
  show();
}

// Send the current buffer to display
void Canvas::show() { show(mat, transform); }

void Canvas::show(const cv::Mat &src, int transform) {
  show(src,
       cv::Rect{0, 0, static_cast<int>(shape().w), static_cast<int>(shape().h)},
       transform);
}

void Canvas::show(const cv::Mat &src, cv::Rect tile, int transform) {
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
}

void Canvas::render(const cv::Mat &src, cv::Point pos, int transform) {
  const auto info = fb.info();
  // Apply transform
  const auto tile_transform = transform ^ this->transform;
  auto dst = transform_mat(src, tile_transform);
  auto corner_pos = transform_point(pos, shape(), tile_transform),
       corner_off = transform_point({0, 0},
                                    {static_cast<unsigned int>(src.cols),
                                     static_cast<unsigned int>(src.rows)},
                                    tile_transform);
  pos = corner_pos - corner_off;
  // Check if trim is necessary
  bool flag_trim = false;
  cv::Rect trim = {0, 0, dst.cols, dst.rows};
  if (dst.cols + pos.x > static_cast<int>(info.width)) {
    trim.width = info.width - pos.x;
    flag_trim = true;
  }
  if (dst.rows + pos.y > static_cast<int>(info.height)) {
    trim.height = info.height - pos.y;
    flag_trim = true;
  }
  if (flag_trim)
    dst = dst(trim);
  // Place the display image to buffer
  if (dst.cols < static_cast<int>(info.width) &&
      dst.rows < static_cast<int>(info.height)) {
    const off_t fb_line_size = info.width * info.bytes_per_pixel,
                x_offset = pos.x * info.bytes_per_pixel,
                y_offset = pos.y * fb_line_size,
                disp_line_size = dst.cols * dst.elemSize();
    for (int n = 0; n < dst.rows; n++) {
      fb.from(dst.data + n * disp_line_size,
              y_offset + n * fb_line_size + x_offset, disp_line_size);
    }
  } else
    // Apply transform
    fb.from(dst.data);
}

} // namespace canvas
