#include "framebuffer.hpp"
#include "vtconsole.hpp"
#include <cstdint>
#include <cstring>
#include <unistd.h>

void clear(fb::FrameBuffer &fb, uint32_t c = 0) {
  uint32_t *buffer = (uint32_t *)fb.buffer();
  for (size_t i = 0; i < fb.info().viewport_pixels; i++) {
    buffer[i] = c;
  }
}

int main(int argc, char **argv) {
  vtconsole::unbind_all();
  fb::FrameBuffer fb(argc > 2 ? argv[1] : "/dev/fb0");
  // for (size_t offset = 0; offset < 1000; offset += 10) {
  //   clear(fb, {.b = 255});
  //   for (size_t j = offset; j < offset + 100; j++) {
  //     size_t line_offset = j * fb.info().width;
  //     for (size_t i = offset; i < offset + 100; i++) {
  //       size_t pixel = i + line_offset;
  //       ((color *)fb.buffer())[pixel] = {.r = 255};
  //     }
  //   }
  // }
  std::cout << "buffer at: " << (void *)fb.buffer() << std::endl;
  clear(fb, 0xFFFFFF);
  sleep(1);
  std::cout << "buffer at: " << (void *)fb.buffer() << std::endl;
  clear(fb, 0xAAAAAA);
  sleep(1);
  std::cout << "buffer at: " << (void *)fb.buffer() << std::endl;
  clear(fb, 0x0);
  sleep(1);
  std::cout << "buffer at: " << (void *)fb.buffer() << std::endl;
  clear(fb, 0xFFAA00);
  sleep(1);
  std::cout << "buffer at: " << (void *)fb.buffer() << std::endl;
  clear(fb, 0xFF << 8);
  sleep(1);
  std::cout << "buffer at: " << (void *)fb.buffer() << std::endl;
  clear(fb, 0xFF << 16);
  sleep(1);
  return 0;
}
