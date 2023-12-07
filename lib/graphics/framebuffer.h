#include <iostream>
#include <linux/fb.h>
#include <opencv2/opencv.hpp>

namespace fb {

typedef struct info {
  uint32_t width, height, line_length;
  uint8_t bytes_per_pixel;
  size_t buf_pixels, buf_bytes;
} info;

class FrameBuffer {
private:
  int fd;
  std::string path;
  // Map size will be 2 times buffer size, since we're using double buffer
  size_t buf_pixels, buf_bytes;
  // Calculated at initialization
  uint8_t bytes_per_pixel;
  // framebuffer will also have 2 times the size of vscreen
  uint8_t *framebuffer = NULL;
  const struct fb_fix_screeninfo finfo = {};
  struct fb_var_screeninfo vinfo, vinfo_bk;

  void init_fd(std::string);

  void buffer_init();

  void buffer_free();

public: // Constructor and Destructor
  FrameBuffer(std::string path);
  FrameBuffer(const FrameBuffer &);
  ~FrameBuffer();

public:
  void reopen();
  // Exposed frame buffer info
  fb::info info();
  // Get pointer to internal buffer
  uint8_t *buffer();
  // Copy from given buffer
  void from(uint8_t *src);
  void from(uint8_t *src, off_t offset, size_t size);
};

} // namespace fb
