#pragma once

#include "shared.h"

#include <opencv2/opencv.hpp>

struct TileReadOut {
  cv::Mat mat;
  cv::Rect bbox;
};

namespace graphics {

typedef enum TileMode { GENERIC, BUTTON, X_SLIDER, Y_SLIDER } TileMode;

typedef struct TileStyle {
  cv::Scalar fill;
  struct {
    int weight;
    cv::Scalar color;
  } outline;
} TileStyle;

typedef struct TileStyleSheet {
  cv::Scalar bg;
  TileStyle normal, active;
  struct {
    double weight;
    double height; // max height relative to cbox height
    double inset;  // relative to height
    cv::Scalar color;
  } text;
} TileStyleSheet;

extern const TileStyleSheet default_style;

class Tile {
  friend class Canvas;

private:
  cv::Point img_offset = {0, 0};
  bool contains(PointerEvent pos);
  TileStyle &sty();

protected:
  cv::Rect bbox; // Bounding box (outer boundary)
                 // used as the boundary of pointer event interception
  cv::Rect cbox; // Content box (content boundary)
                 // used as the boundary of content rendering
                 // offset is relative to bounding box
  // Flag to indicate the need for re-rendering
  bool updated = false;
  // Flag to indicate if latest canvas has been read
  bool readout = false;
  // Flag to indicate the tile is active
  // `active` is set to true when pointer button down withing its bbox.
  //          and set to false when pointer button up.
  // If pointer drags outside bbox without button up, `active` remains true.
  bool active = false;
  // Read out the canvas and set readout flag.
  // Returns true if new data is available for read, otherwise false;
  std::shared_ptr<const TileReadOut> buffer = nullptr;
  std::shared_ptr<const TileReadOut> read(bool once = true);

public:
  Tile(cv::Rect bbox, int pad = 0, TileMode mode = TileMode::GENERIC);
  TileMode mode = TileMode::GENERIC;
  Tile &as(TileMode mode);
  // Color palette for decoration rendering
  TileStyleSheet style = default_style;
  Tile &use(TileStyleSheet &style);
  Tile &use(TileStyleSheet &&style);
  // Will be called when the tile is updated
  // behavior varies depending on the tile mode
  // GENERIC: handler is called whenever an active event is intercepted
  // BUTTON: handler is called when button is released
  std::function<void(Tile &, bool)> handler = [](Tile &, bool) {};
  Tile &use(std::function<void(Tile &, bool)> &handler);
  Tile &use(std::function<void(Tile &, bool)> &&handler);
  // Returns true if new data is used for rendering, otherwise false.
  bool auto_raster = true;
  bool raster();
  // Return a new tile with the same style and handler, but different bbox
  Tile &loc(cv::Rect bbox, int pad = 0);
  // Content rendering properties and methods
  cv::Mat bg, fg;
  Tile &wipe();
  Tile &fill(cv::Mat img);
  Tile &fill(double x2 = 1.0, double y2 = 1.0);
  Tile &fill(double x1, double x2, double y1, double y2);
  Tile &text(std::string text);
  // Interaction related properties and methods
  bool is_active();
  cv::Point2d val = {0, 0};
  Tile &use(cv::Point2d &val);
  Tile &use(cv::Point2d &&val);
  cv::Point2d relative(int x, int y);
  // Handle current pointer position, returns if the event is intercepted
  bool handle(PointerEvent pos);
  bool render(bool force = false, bool state_change = false);
};

} // namespace graphics
