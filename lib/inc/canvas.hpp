#include "framebuffer.hpp"
#include <opencv2/opencv.hpp>
#include <stdint.h>
#include <string>
#include <sys/types.h>

namespace canvas {

typedef struct {
  unsigned int w;
  unsigned int h;
} shape;

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
  fb::FrameBuffer fb;
  fb::info fb_info;
  cv::Mat mat;
  int transform = transform::NONE;
  void constructor(std::string fb_path, int transform);

public:
  Canvas(std::string path, int transform = transform::NONE);
  int get_transform();
  int set_transform(int);
  // Create a fork of framebuffer in new process
  int fork();
  // Get the framebuffer
  cv::Mat Mat();
  // Shape of the canvas (after transformation)
  canvas::shape shape();
  // Set the color for entire buffer
  void clear();
  void clear(uint8_t);
  void clear(cv::Scalar);
  // Send the current buffer to display
  void show();
  // Use provided mat instead
  void show(const cv::Mat &, int transform = transform::NONE);
  // Specify a region on display to project to
  void show(const cv::Mat &, cv::Rect, int transform = transform::NONE);
  // Render to framebuffer without any transformation
  void render(const cv::Mat &, cv::Point pos = {0, 0}, int transform = transform::NONE);
};

} // namespace canvas
