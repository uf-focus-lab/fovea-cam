#include "canvas.h"

#include <cstring>
#include <opencv2/opencv.hpp>
#include <unistd.h>

namespace graphics {

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

cv::Point transform_point(cv::Point p, Shape s, const int &transform) {
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

Canvas::Canvas(unsigned width, unsigned height) : mat(height, width, CV_8UC4) {
  this->width = width;
  this->height = height;
}

Canvas::Canvas(unsigned width, unsigned height, unsigned line_length)
    : mat(height, line_length, CV_8UC4) {
  this->width = width;
  this->height = height;
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

Shape Canvas::shape() {
  if (transform & transform::TRANSPOSE) {
    return {.w = height, .h = width};
  } else {
    return {.w = width, .h = height};
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
  show(src,
       cv::Rect{0, 0, static_cast<int>(shape().w), static_cast<int>(shape().h)},
       transform);
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

Canvas &Canvas::render(const cv::Mat &src, cv::Point pos, int transform) {
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
  return *this;
}

Canvas &Canvas::apply(void *fb) {
  const auto s = mat.size();
  memcpy(fb, mat.data, s.height * s.width * 4);
  return *this;
}

} // namespace graphics
