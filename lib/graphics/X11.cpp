#include "X11.h"

#include <X11/X.h>
#include <cstring>
#include <glob.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#undef LOG_NAME
#define LOG_NAME "[graphics::X11] "

XVisualInfo *getVisualInfo(Display *display, int screen_number) {
  auto vinfo = new XVisualInfo;
  if (!XMatchVisualInfo(display, screen_number, 24, TrueColor, vinfo)) {
    fprintf(stderr, "No matching visual configuration\n");
    return nullptr;
  }
  return vinfo;
}

unsigned int buttonToMask(unsigned int btn) {
  return (((btn & Button1Mask) ? 1 : 0) << 1) |
         (((btn & Button2Mask) ? 1 : 0) << 2) |
         (((btn & Button3Mask) ? 1 : 0) << 3) |
         (((btn & Button4Mask) ? 1 : 0) << 4) |
         (((btn & Button5Mask) ? 1 : 0) << 5);
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
      fprintf(stderr, LOG_NAME "Cannot get root window\n");
      return;
    } else {
      fprintf(stderr, LOG_NAME "Got root window: %ld\n", window);
    }
    if (!XMatchVisualInfo(display, screen_number, 24, TrueColor, &vinfo)) {
      fprintf(stderr, "No matching visual configuration\n");
      return;
    } else {
      fprintf(stderr, LOG_NAME "Got visual info: %d\n", vinfo.depth);
    }
    XWindowAttributes window_attributes;
    XGetWindowAttributes(display, window, &window_attributes);
    width = window_attributes.width;
    height = window_attributes.height;
    const size_t bytes_per_line = width * 4;
    fb_bytes = height * bytes_per_line;
    fb = (unsigned char *)malloc(fb_bytes);
    img = XCreateImage(display, vinfo.visual, vinfo.depth, ZPixmap, 0,
                       (char *)fb, width, height, 8, bytes_per_line);
    if (img == 0) {
      fprintf(stderr, LOG_NAME "XImage is null!\n");
      return;
    } else {
      fprintf(stderr, LOG_NAME "Display Size: %d x %d\n", img->width,
              img->height);
    }
    XSelectInput(display, window,
                 ExposureMask | KeyPressMask | KeyReleaseMask |
                     PointerMotionMask | ButtonPressMask | ButtonReleaseMask);
    XGCValues gcv = {.graphics_exposures = 0};
    gc = XCreateGC(display, window, GCGraphicsExposures, &gcv);
    XMapWindow(display, window);
    open = true;
  };

  graphics::PointerEvent wait_pointer(bool block) {
    if (!block && !XEventsQueued(display, QueuedAfterFlush)) {
      return graphics::PointerEvent({false});
    }
    static XEvent e;
    graphics::PointerEvent pe = {.valid = false, .button_mask = 0};
    bool flag_return = false;
    // Find the LAST event that needs to be processed, skip move events for
    // faster response
    while (!flag_return && XEventsQueued(display, QueuedAfterFlush)) {
      XNextEvent(display, &e); // This is always blocking
      switch (e.type) {
      case MotionNotify:
        pe.x = e.xmotion.x;
        pe.y = e.xmotion.y;
        pe.button_state = buttonToMask(e.xmotion.state);
        break;
      case ButtonPress:
        pe.x = e.xbutton.x;
        pe.y = e.xbutton.y;
        pe.button_mask = 1 << e.xbutton.button;
        pe.button_state = buttonToMask(e.xbutton.state) | pe.button_mask;
        flag_return = true;
        break;
      case ButtonRelease:
        pe.x = e.xbutton.x;
        pe.y = e.xbutton.y;
        pe.button_mask = 1 << e.xbutton.button;
        pe.button_state = buttonToMask(e.xbutton.state) & ~pe.button_mask;
        flag_return = true;
        break;
      default:
        continue;
      }
      pe.valid = true;
    }
    return pe;
  }

  ~IMPL(){
      // if (img != NULL)
      //   XDestroyImage(img);
      // if (fb != NULL)
      //   free(fb);
      // if (gc != NULL)
      //   XFreeGC(display, gc);
      // if (window != 0)
      //   XDestroyWindow(display, window);
      // if (display != NULL)
      //   XCloseDisplay(display);
  };
  cv::Size shape() {
    return {static_cast<int>(width), static_cast<int>(height)};
  };
  void use(void *buffer){/* TODO */};
  void sync() {
    XPutImage(display, window, gc, img, 0, 0, 0, 0, width, height);
  };
  void flush() { XFlush(display); }
  bool is_open() { return open; };
  unsigned char *buffer() { return (unsigned char *)fb; };
};

namespace graphics {

X11FB::X11FB() : impl(new IMPL()){};

X11FB::~X11FB() { delete (IMPL *)impl; }

cv::Size X11FB::shape() { return ((IMPL *)impl)->shape(); }

void X11FB::use(void *buffer) { ((IMPL *)impl)->use(buffer); }

void X11FB::sync() { ((IMPL *)impl)->sync(); }

void X11FB::flush() { ((IMPL *)impl)->flush(); }

bool X11FB::is_open() { return ((IMPL *)impl)->is_open(); }

unsigned char *X11FB::buffer() { return ((IMPL *)impl)->buffer(); }

PointerEvent X11FB::wait_pointer(bool block) {
  return ((IMPL *)impl)->wait_pointer(block);
}

static const char *const argv_dmps[] = {"xset", "-dpms", NULL};
static const char *const argv_soff[] = {"xset", "s", "off", NULL};
static const char *const argv_nblk[] = {"xset", "s", "noblank", NULL};

int xset() {
  if (fork() == 0) {
    execvp(argv_dmps[0], (char *const *)argv_dmps);
    exit(0);
  }
  if (fork() == 0) {
    execvp(argv_soff[0], (char *const *)argv_soff);
    exit(0);
  }
  if (fork() == 0) {
    execvp(argv_nblk[0], (char *const *)argv_nblk);
    exit(0);
  }
  return 0;
}

int x11env() {
  glob_t glob_result;
  memset(&glob_result, 0, sizeof(glob_result));
  const int ret = glob("/tmp/.X11-unix/X*", 0, nullptr, &glob_result);
  if (ret) {
    std::cerr << LOG_NAME "Error in glob() call" << std::endl;
    globfree(&glob_result);
    return 1;
  }
  for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
    std::string file_path = glob_result.gl_pathv[i];
    if (access(file_path.c_str(), W_OK) == 0) {
      // Extracting the DISPLAY number from the file path
      size_t last_slash_pos = file_path.rfind('/');
      if (last_slash_pos != std::string::npos) {
        std::string display_number = file_path.substr(last_slash_pos + 2);
        std::cerr << LOG_NAME "Using DISPLAY :" << display_number << std::endl;
        setenv("DISPLAY", (":" + display_number).c_str(), 1);
        globfree(&glob_result);
        return xset();
      } else {
        std::cerr << LOG_NAME "Bad display path: " << file_path << std::endl;
      }
    } else {
      std::cerr << LOG_NAME "Display not writable: "
                << basename(file_path.c_str()) << std::endl;
    }
  }
  globfree(&glob_result);
  return 1;
}

} // namespace graphics
