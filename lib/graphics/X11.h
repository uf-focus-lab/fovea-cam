#include "fb.h"
#include "shared.h"

#pragma once

namespace graphics {

int x11env();

class X11FB : public FB {
private:
  void *impl;

public:
  X11FB();
  ~X11FB();
  cv::Size shape();
  unsigned char *buffer();
  void sync();
  void flush();
  void use(void *buffer);
  bool is_open();
  PointerEvent wait_pointer(int timeout_ms = 16);
};

} // namespace graphics
