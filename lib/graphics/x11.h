#include "canvas.h"

#pragma once

typedef struct {
  bool valid;
  int x;
  int y;
  unsigned int button;
} PointerEvent;

namespace graphics {

int x11env();

class X11FB {
private:
  void *impl;

public:
  X11FB();
  ~X11FB();
  Shape shape();
  unsigned char *buffer();
  void sync();
  void flush();
  void use(void *buffer);
  bool isOpen();
  PointerEvent wait_pointer(bool block = true);
  int events_pending();
};

} // namespace graphics
