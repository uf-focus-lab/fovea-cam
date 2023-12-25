#include "canvas.h"

#pragma once

namespace graphics {
class X11FB {
private:
  void *impl;

public:
  X11FB();
  ~X11FB();
  Shape shape();
  unsigned char *buffer();
  void sync();
  void use(void *buffer);
  bool isOpen();
};

} // namespace graphics
