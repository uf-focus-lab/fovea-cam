#include <SDL2/SDL.h>

#include "fb.h"
#include "shared.h"

#pragma once

namespace graphics {

class SDL2FB : public FB {
public:
  SDL2FB(std::string name = "FoveaCam Duo");
  ~SDL2FB();
  cv::Size shape();
  unsigned char *buffer();
  void sync();
  void flush();
  void use(void *buffer);
  bool is_open();
  PointerEvent wait_pointer(int timeout_ms = 16);

private:
  const std::string name;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Event event;
  const unsigned width = 1440, height = 2160;
  cv::Mat fb = cv::Mat(height, width, CV_8UC4);
  unsigned button_state;
  void on_button(PointerEvent &event);
  float dpi_scale;
};

} // namespace graphics
