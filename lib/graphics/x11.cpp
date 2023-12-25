#include "x11.h"

#include <cstring>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

XVisualInfo *getVisualInfo(Display *display, int screen_number) {
  auto vinfo = new XVisualInfo;
  if (!XMatchVisualInfo(display, screen_number, 24, TrueColor, vinfo)) {
    fprintf(stderr, "No matching visual configuration\n");
    return nullptr;
  }
  return vinfo;
}

class IMPL {
private:
  Display *display;
  int screen_number;
  Screen *screen;
  Window window;
  XVisualInfo vinfo;
  unsigned width, height, fb_bytes;
  // mapped framebuffer
  unsigned char *fb = NULL;
  XImage *img = NULL;
  GC gc;
  // indicates if the window is successfully opened
  bool open = false;

public:
  IMPL() {
    display = XOpenDisplay(NULL);
    screen_number = DefaultScreen(display);
    screen = XScreenOfDisplay(display, screen_number);
    window = XRootWindowOfScreen(screen);
    if (window == 0) {
      fprintf(stderr, "Cannot get root window\n");
      return;
    } else {
      printf("[X11FB] Got root window: %ld\n", window);
    }
    XSync(display, True);
    if (!XMatchVisualInfo(display, screen_number, 24, TrueColor, &vinfo)) {
      fprintf(stderr, "No matching visual configuration\n");
      return;
    } else {
      printf("[X11FB] Got visual info: %d\n", vinfo.depth);
    }
    XWindowAttributes window_attributes;
    XGetWindowAttributes(display, window, &window_attributes);
    width = window_attributes.width;
    height = window_attributes.height;
    const size_t bytes_per_line = width * 4;
    fb_bytes = height * bytes_per_line;
    fb = (unsigned char *)malloc(fb_bytes);
    XImage *img = XCreateImage(display, vinfo.visual, vinfo.depth, ZPixmap, 0,
                               (char *)fb, width, height, 8, bytes_per_line);
    if (img == 0) {
      fprintf(stderr, "XImage is null!\n");
      return;
    } else {
      printf("[X11FB] Display Size: %d x %d\n", img->width, img->height);
    }
    XSync(display, True);
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    XGCValues gcv = {.graphics_exposures = 0};
    gc = XCreateGC(display, window, GCGraphicsExposures, &gcv);
    XMapWindow(display, window);
    open = true;
  };
  ~IMPL() {
    if (img != NULL)
      XDestroyImage(img);
    if (fb != NULL)
      free(fb);
    if (gc != NULL)
      XFreeGC(display, gc);
    if (window != 0)
      XDestroyWindow(display, window);
    if (display != NULL)
      XCloseDisplay(display);
  };
  graphics::Shape shape() { return graphics::Shape{width, height}; };
  void use(void *buffer){/* TODO */};
  void sync() {
    XPutImage(display, window, gc, img, 0, 0, 0, 0, width, height);
  };
  bool isOpen() { return open; };
  unsigned char *buffer() { return (unsigned char *)fb; };
};

namespace graphics {
X11FB::X11FB() : impl(new IMPL()){};
X11FB::~X11FB() { delete (IMPL *)impl; }
Shape X11FB::shape() { return ((IMPL *)impl)->shape(); }
void X11FB::use(void *buffer) { ((IMPL *)impl)->use(buffer); }
void X11FB::sync() { ((IMPL *)impl)->sync(); }
bool X11FB::isOpen() { return ((IMPL *)impl)->isOpen(); }
unsigned char *X11FB::buffer() { return ((IMPL *)impl)->buffer(); };
} // namespace graphics
