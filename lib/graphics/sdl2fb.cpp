#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "global.h"
#include "sdl2fb.h"
#include "shared.h"
#include "util/time.h"

#define LOG_NAME "[graphics::SDL2FB] "

namespace graphics {

inline void trigger(unsigned &next, unsigned digit, unsigned type) {
  if (type == SDL_MOUSEBUTTONDOWN) {
    next |= (1 << digit);
  } else {
    next &= ~(1 << digit);
  }
}

void SDL2FB::on_button(PointerEvent &pe) {
  auto const prev = button_state;
  switch (event.button.button) {
  case SDL_BUTTON_LEFT:
    trigger(button_state, 1, event.button.type);
    break;
  case SDL_BUTTON_RIGHT:
    trigger(button_state, 2, event.button.type);
    break;
  case SDL_BUTTON_MIDDLE:
    trigger(button_state, 3, event.button.type);
    break;
  }
  pe.button_state = button_state;
  pe.button_mask = pe.button_state ^ prev;
}

SDL2FB::SDL2FB(std::string name) : name(name) {
  std::cerr << LOG_NAME "Creating SDL2FB window \"" << name << "\""
            << std::endl;
  window = SDL_CreateWindow(name.c_str(), SDL_WINDOWPOS_UNDEFINED,
                            SDL_WINDOWPOS_UNDEFINED, width / 2, height / 2,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    throw std::runtime_error(LOG_NAME "Failed to create SDL2 window: " +
                             std::string(SDL_GetError()));
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    SDL_DestroyWindow(window);
    throw std::runtime_error(LOG_NAME "Failed to create SDL2 renderer: " +
                             std::string(SDL_GetError()));
  }
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!texture) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    throw std::runtime_error(LOG_NAME "Failed to create SDL2 texture: " +
                             std::string(SDL_GetError()));
  }
  // Check DPI scaling
  int win_w, win_h, draw_w, draw_h;
  SDL_GetWindowSize(window, &win_w, &win_h);             // Logical points
  SDL_GetRendererOutputSize(renderer, &draw_w, &draw_h); // Physical pixels
  dpi_scale = draw_w / (float)win_w;
  std::cerr << LOG_NAME "Window size: " << win_w << "x" << win_h << std::endl
            << "                   drawable size: " << draw_w << "x" << draw_h
            << std::endl
            << "                   DPI scale: " << dpi_scale << "\n";
}

SDL2FB::~SDL2FB() {
  if (texture) {
    SDL_DestroyTexture(texture);
  }
  if (renderer) {
    SDL_DestroyRenderer(renderer);
  }
  if (window) {
    SDL_DestroyWindow(window);
  }
  SDL_Quit();
  std::cerr << LOG_NAME "Exited gracefully." << std::endl;
}

cv::Size SDL2FB::shape() { return cv::Size(width, height); }

unsigned char *SDL2FB::buffer() { return fb.data; }

void SDL2FB::sync() {
  SDL_UpdateTexture(texture, nullptr, fb.data, fb.step[0]);
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, nullptr, nullptr);
  SDL_RenderPresent(renderer);
}

void SDL2FB::flush() {
  std::memset(fb.data, 0, fb.total() * fb.elemSize());
  sync();
}

void SDL2FB::use(void *buffer) {
  throw std::runtime_error(LOG_NAME
                           "use() method is not implemented for SDL2FB");
}

bool SDL2FB::is_open() { return true; }

PointerEvent SDL2FB::wait_pointer(int timeout_ms) {
  const auto ddl = Time::ms() + timeout_ms;
  PointerEvent pe = {true, 0, 0, 0, 0};
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_QUIT:
      global::flag_term = true;
      pe.valid = false;
      return pe;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE ||
          event.key.keysym.sym == SDLK_q) {
        global::flag_term = true;
        pe.valid = false;
        return pe;
      }
    case SDL_MOUSEMOTION:
      pe.x = static_cast<unsigned>(event.motion.x * dpi_scale);
      pe.y = static_cast<unsigned>(event.motion.y * dpi_scale);
      pe.button_state = button_state;
      return pe;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      pe.x = static_cast<unsigned>(event.button.x * dpi_scale);
      pe.y = static_cast<unsigned>(event.button.y * dpi_scale);
      on_button(pe);
      return pe;
    }
    if (Time::ms() > ddl)
      break;
  }
  return {false, 0, 0, 0, 0};
}

} // namespace graphics
