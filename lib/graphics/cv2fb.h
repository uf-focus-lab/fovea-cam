#include "fb.h"
#include "shared.h"
#include <deque>

#pragma once

namespace graphics {

typedef struct Pointer_s {
  std::deque<PointerEvent> queue;
  // Pointer move event will reuse pointer state;
  unsigned state = 0;
} Pointer;

class CV2FB : public FB {
public:
  CV2FB(std::string name = "FoveaCam Duo");
  ~CV2FB();
  cv::Size shape();
  unsigned char *buffer();
  void sync();
  void flush();
  void use(void *buffer);
  bool is_open();
  PointerEvent wait_pointer(int timeout_ms = 16);

private:
  const std::string name;
  const unsigned width = 1440, height = 2160;
  cv::Mat fb = cv::Mat(height, width, CV_8UC4);
  Pointer pointer;
};

} // namespace graphics
