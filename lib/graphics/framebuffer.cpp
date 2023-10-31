#include "framebuffer.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <opencv2/core/mat.hpp>
#include <stdexcept>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define CHECK(VAL)                                                             \
  if (!(VAL)) {                                                                \
    throw std::runtime_error(std::strerror(errno));                            \
  }

namespace fb {

void FrameBuffer::init_fd(std::string path) {
  /* Open the file for reading and writing */
  fd = open(path.c_str(), O_RDWR | O_SYNC);
  CHECK(fd >= 0);
  std::cout << "Successfully opened " << path << std::endl;
  /* Get finfo and vinfo from fd */
  CHECK(ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == 0);
  CHECK(ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) == 0);
  /* Check if using 32 bit pixels */
  switch (vinfo.bits_per_pixel) {
  case 24:
  case 32:
    break;
  default:
    throw std::runtime_error("Only 32 bit pixel format is currently supported");
  }
}

void FrameBuffer::buffer_init() {
  const auto info = this->info();
  /* Compute the byte size of the buffer */
  bytes_per_pixel = vinfo.bits_per_pixel / 8;
  viewport_pixels = info.width * info.height;
  viewport_bytes = viewport_pixels * bytes_per_pixel;
  /* Map memory accordingly */
  // framebuffer = (uint8_t *)mmap(NULL, viewport_bytes, PROT_READ | PROT_WRITE,
  //                               MAP_SHARED, fd, 0);
  // if (framebuffer == MAP_FAILED) {
  //   std::cerr
  //       << "[framebuffer::ERROR] Failed to map framebuffer device to memory."
  //       << std::endl;
  // throw std::runtime_error("");
  // }
}

void FrameBuffer::buffer_free() {
  // blank screen
  // memset(framebuffer, 0, viewport_bytes);
  // release mapped memory
  if (framebuffer != nullptr)
    munmap((void *)framebuffer, viewport_bytes);
}

FrameBuffer::FrameBuffer(std::string path) {
  this->path = path;
  init_fd(this->path);
  buffer_init();
  // memset(framebuffer, 0, viewport_bytes);
#ifdef DEBUG
  std::cout << "xres           " << vinfo.xres << std::endl
            << "yres           " << vinfo.yres << std::endl
            << "xres_virtual   " << vinfo.xres_virtual << std::endl
            << "yres_virtual   " << vinfo.yres_virtual << std::endl
            << "xoffset        " << vinfo.xoffset << std::endl
            << "yoffset        " << vinfo.yoffset << std::endl
            << "line_length    " << finfo.line_length << std::endl
            << "bits_per_pixel " << vinfo.bits_per_pixel << std::endl
            << "offset red     " << vinfo.red.offset << std::endl
            << "offset green   " << vinfo.green.offset << std::endl
            << "offset blue    " << vinfo.blue.offset << std::endl
            << "offset transp  " << vinfo.transp.offset << std::endl
            << "buffer size    " << viewport_bytes << std::endl;
#endif
}

FrameBuffer::FrameBuffer(const FrameBuffer &) {
  std::cerr << "[framebuffer::ERROR] Copy constructor is not allowed."
            << std::endl
            << "    Pass by reference only." << std::endl;
  throw std::runtime_error("");
}

FrameBuffer::~FrameBuffer() {
  buffer_free();
  close(fd);
}

void FrameBuffer::reopen() { fd = open(path.c_str(), O_RDWR | O_SYNC); }

info FrameBuffer::info() {
  return {.width = vinfo.xres_virtual,
          .height = vinfo.yres_virtual,
          .bytes_per_pixel = bytes_per_pixel,
          .viewport_pixels = viewport_pixels,
          .viewport_bytes = viewport_bytes};
}

uint8_t *FrameBuffer::buffer() { return framebuffer; }

void FrameBuffer::from(uint8_t *src) { from(src, 0, viewport_bytes); }

void FrameBuffer::from(uint8_t *src, off_t offset, size_t size) {
  // Move cursor to the beginning of the buffer
  lseek(fd, offset, SEEK_SET);
  write(fd, src, size);
}

} // namespace fb
