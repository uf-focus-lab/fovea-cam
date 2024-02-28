#include <opencv2/opencv.hpp>
#include <stdint.h>
#include <sys/types.h>
#include <vector>

#include "X11.h"
#include "graphics/shared.h"
#include "tile.h"

#pragma once

namespace graphics {

typedef enum {
  NONE = 0b000,
  // Basic operations
  FLIP_X = 0b100,
  FLIP_Y = 0b010,
  TRANSPOSE = 0b001,
  // Combinations (aliases)
  ROTATE_90 = 0b011,
  ROTATE_270 = 0b101,
  FLIP_XY = 0b110,
  ROTATE_180 = 0b110,
} transform;

class Canvas {
private:
  cv::Mat mat, cursor_up, cursor_down;
  int cursor_size = 0;
  int transform = transform::NONE;
  unsigned int width, height;
  PointerEvent *pe = nullptr;
  void cursor_init(int size);
  cv::Mat handle_pointer(const PointerEvent *e);

public:
  Canvas(cv::Size size);
  Canvas(cv::Size size, unsigned line_length);
  Canvas(unsigned width, unsigned height);
  Canvas(unsigned width, unsigned height, unsigned line_length);
  ~Canvas();
  int get_transform();
  int set_transform(int);
  // Create a fork of framebuffer in new process
  int fork();
  // Get the framebuffer
  cv::Mat Mat();
  // Shape of the canvas (after transformation)
  cv::Size shape();
  // Set the color for entire buffer
  Canvas &clear();
  Canvas &clear(uint8_t);
  Canvas &clear(cv::Scalar);
  // Send the current buffer to display
  Canvas &show();
  // Use provided mat instead
  Canvas &show(const cv::Mat &, int transform = transform::NONE);
  // Specify a region on display to project to
  Canvas &show(const cv::Mat &, cv::Rect, int transform = transform::NONE);
  // Shortcut to show tile(s)
  Canvas &show(Tile &, bool force = false, int transform = transform::NONE);
  Canvas &show(std::vector<Tile *> &, bool force = false, int transform = transform::NONE);
  // Render to internal buffer without any transformation
  Canvas &render(const cv::Mat &, cv::Point pos = {0, 0},
                 int transform = transform::NONE);
  // Apply internal buffer to framebuffer
  Canvas &apply(void *, const PointerEvent *event = nullptr);
  Canvas &apply(X11FB &fb, const PointerEvent *event = nullptr);
};

} // namespace graphics
