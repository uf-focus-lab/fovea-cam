#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "cv2fb.h"
#include "global.h"
#include "shared.h"
#include "util/time.h"

#define LOG_NAME "[graphics::cv2fb] "

namespace graphics {

unsigned int mouseFlagsToState(unsigned int flags) {
  return ((flags & cv::EVENT_FLAG_LBUTTON) ? 2 : 0) |
         ((flags & cv::EVENT_FLAG_RBUTTON) ? 4 : 0) |
         ((flags & cv::EVENT_FLAG_MBUTTON) ? 8 : 0);
}

void onMouse(int event, int x, int y, int flags, void *userdata) {
  auto &pointer = *static_cast<Pointer *>(userdata);
  auto state = mouseFlagsToState(flags);
  auto mask = state ^ pointer.state;
  // Manual capture events
  if (event == cv::EVENT_LBUTTONDOWN) {
    mask |= 2; // Left button down
    state |= 2;
  }
  if (event == cv::EVENT_LBUTTONUP) {
    mask |= 2; // Left button up
    state &= ~2;
  }
  std::cerr << LOG_NAME "Mouse event: " << event << ", x: " << x << ", y: " << y
            << ", flags: " << flags << ", mask: " << mask
            << ", state: " << state << std::endl;
  // Ignore irrelevant event
  if (mask == 0 && event != cv::EVENT_MOUSEMOVE)
    return;
  pointer.state = state;
  pointer.queue.push_back({true, x, y, state, mask});
}

CV2FB::CV2FB(std::string name) : name(name) {
  std::cerr << LOG_NAME "Initializing CV2FB with name: " << name << std::endl;
  cv::namedWindow(name, cv::WINDOW_NORMAL | cv::WINDOW_GUI_NORMAL);
  cv::resizeWindow(name, width, height);
  cv::setMouseCallback(name, onMouse, &this->pointer);
}

CV2FB::~CV2FB() { cv::destroyWindow(name); }

cv::Size CV2FB::shape() { return cv::Size(width, height); }

unsigned char *CV2FB::buffer() { return fb.data; }

void CV2FB::sync() { cv::imshow(name, fb); }

void CV2FB::flush() {
  std::memset(fb.data, 0, fb.total() * fb.elemSize());
  sync();
}

void CV2FB::use(void *buffer) {
  throw std::runtime_error(LOG_NAME
                           "use() method is not implemented for CV2FB");
}

bool CV2FB::is_open() { return true; }

PointerEvent CV2FB::wait_pointer(int timeout_ms) {
  const auto ddl = Time::ms() + timeout_ms;
  while (1) {
    while (!pointer.queue.empty()) {
      PointerEvent pe = pointer.queue.front();
      pointer.queue.pop_front();
      if (pe.valid)
        return pe;
    }
    auto key = cv::waitKey(1);
    if (key == 27 || key == 'q') { // ESC or 'q'
      global::flag_term = true;
    }
    if (Time::ms() > ddl)
      return {false, 0, 0, 0, 0};
  }
}

} // namespace graphics
