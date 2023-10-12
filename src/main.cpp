#include "canvas.hpp"
#include "vtconsole.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <opencv2/core/types.hpp>
#include <string>
#include <unistd.h>

namespace Color {
const cv::Scalar black(0, 0, 0, 0), white(255, 255, 255, 0), blue(255, 0, 0, 0),
    green(0, 255, 0, 0), red(0, 0, 255, 0);
}

void draw(canvas::Canvas &canvas) {
  const auto shape = canvas.shape();
  struct {
    double draw = 0, paint = 0;
  } duration;
  size_t count = 0;
  for (int i = 0; (i < (int)shape.h) && (i < (int)shape.w); i += 5) {
    auto start = std::chrono::system_clock::now();
    canvas.clear(Color::blue);
    cv::rectangle(canvas.Mat(), {i, i}, {i + 100, i + 100}, Color::red,
                  cv::FILLED);
    auto check_a = std::chrono::system_clock::now();
    canvas.show();
    auto check_b = std::chrono::system_clock::now();
    duration.draw += std::chrono::duration<double>(check_a - start).count();
    duration.paint += std::chrono::duration<double>(check_b - check_a).count();
    double time = std::chrono::duration<double>(check_b - start).count();
    if (time < 0.033) {
      usleep(33000 - (int)(time * 1000000));
    }
    count++;
  }
  duration.draw /= count;
  duration.paint /= count;
  auto total = duration.draw + duration.paint;
  std::cout << std::endl
            << "framerate: " << 1 / total << " fps" << std::endl
            << "  > draw : " << 1000 * duration.draw << "ms\t"
            << duration.draw / total * 100 << "%" << std::endl
            << "  > paint: " << 1000 * duration.paint << "ms\t"
            << duration.paint / total * 100 << "%" << std::endl
            << std::endl;
  // std::string line;
  // std::getline(std::cin, line);
  // usleep(800000);
}

int main(int argc, char *argv[]) {
  // if (argc < 2) {
  //   printf("Usage: %s <fb path>\n", argv[0]);
  //   throw;
  // }
  // Unbind all vtconsole from frame buffer
  vtconsole::unbind_all();
  // Create canvas for given framebuffer
  canvas::Canvas canvas(argc > 2 ? argv[1] : "/dev/fb0");
  // Draw something
  std::cout << "transform::NONE" << std::endl;
  canvas.set_transform(canvas::transform::NONE);
  draw(canvas);

  std::cout << "transform::ROTATE_90" << std::endl;
  canvas.set_transform(canvas::transform::ROTATE_90);
  draw(canvas);

  std::cout << "transform::ROTATE_180" << std::endl;
  canvas.set_transform(canvas::transform::ROTATE_180);
  draw(canvas);

  std::cout << "transform::ROTATE_270" << std::endl;
  canvas.set_transform(canvas::transform::ROTATE_270);
  draw(canvas);

  // fb.render(canvas::rect{.a = p[3][0], .b = p[5][2]}, Color::black);
  return 0;
}
