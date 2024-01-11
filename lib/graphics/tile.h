#include "shared.h"
#include <opencv2/opencv.hpp>

#pragma once
namespace graphics {

class Tile {
  friend class Canvas;

private:
  cv::Point img_offset = {0, 0};
  void init();
  bool contains(PointerEvent pos);
  struct {
    std::string (*cb)(cv::Point2d);
    cv::Scalar color;
    double height;
    double pad;
  } text_renderer = {.cb = nullptr};

protected:
  cv::Rect bbox;
  bool updated = false;
  bool active = false;
  cv::Mat raster();

public:
  cv::Mat bg, fg, rasterized;
  cv::Point2d val = {0, 0};
  Tile(cv::Rect bbox, int pad = 0);
  cv::Rect loc();
  cv::Rect loc(cv::Rect bbox, int pad = 0);
  cv::Point2d relative(int x, int y);
  Tile &fill(cv::Mat img);
  Tile &fill(cv::Scalar color = color::black(255), double x2 = 1,
             double y2 = 1);
  Tile &fill(cv::Scalar color, double x1, double x2, double y1, double y2);
  Tile &text(std::string text, cv::Scalar color, double height = 0.6,
             double pad = 0.2);
  Tile &text(std::string text, cv::Scalar color, unsigned height, double pad);
  Tile &text(std::string (*cb)(cv::Point2d), cv::Scalar color,
             double height = 0.6, double pad = 0.2);
  bool handle(PointerEvent pos);
};

} // namespace graphics
