#include <opencv2/opencv.hpp>
#include <stdint.h>
#include <string>
#include <sys/types.h>

#pragma once

namespace graphics {

typedef struct {
  unsigned int w;
  unsigned int h;
} Shape;

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
  cv::Mat mat;
  int transform = transform::NONE;
  void constructor(std::string fb_path, int transform);
  unsigned int width, height;

public:
  Canvas(unsigned, unsigned);
  Canvas(unsigned, unsigned, unsigned);
  int get_transform();
  int set_transform(int);
  // Create a fork of framebuffer in new process
  int fork();
  // Get the framebuffer
  cv::Mat Mat();
  // Shape of the canvas (after transformation)
  Shape shape();
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
  // Render to internal buffer without any transformation
  Canvas &render(const cv::Mat &, cv::Point pos = {0, 0},
                 int transform = transform::NONE);
  // Apply internal buffer to framebuffer
  Canvas &apply(void *);
};

} // namespace graphics
