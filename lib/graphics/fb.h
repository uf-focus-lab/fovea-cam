#pragma once

#include "shared.h"
#include <opencv2/core.hpp>

namespace graphics {

class FB {
public:
  virtual ~FB() {}
  virtual cv::Size shape() = 0;
  virtual unsigned char *buffer() = 0;
  virtual void sync() = 0;
  virtual void flush() = 0;
  virtual void use(void *buffer) = 0;
  virtual bool is_open() = 0;
  virtual PointerEvent wait_pointer(int timeout_ms = 16) = 0;
};

} // namespace graphics
