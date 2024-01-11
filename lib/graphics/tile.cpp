#include "tile.h"
#include "alpha.h"
#include "shared.h"
#include "util/clamp.h"
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#undef LOG_NAME
#define LOG_NAME "[graphics::Tile] "

namespace graphics {

void Tile::init() {
  const int h = bbox.height, w = bbox.width;
  bg = cv::Mat(h, w, CV_8UC4, color::black(255));
  fg = cv::Mat(h, w, CV_8UC4, color::black(0));
  img_offset = {0, 0};
}

bool Tile::contains(PointerEvent pos) {
  return pos.x >= bbox.x && pos.x < bbox.x + bbox.width && pos.y >= bbox.y &&
         pos.y < bbox.y + bbox.height;
}

Tile::Tile(cv::Rect box, int p)
    : bbox({box.x + p, box.y + p, box.width - p * 2, box.height - p * 2}) {
  init();
}

bool Tile::is_active() { return active; }

// bool Tile::button(PointerEvent *pos, cv::Scalar bg_normal,
//                   cv::Scalar bg_active) {
//   auto _active = active;
// }

cv::Rect Tile::loc() { return cv::Rect(bbox); }
cv::Rect Tile::loc(cv::Rect box, int pad) {
  bbox = {box.x + pad, box.y + pad, box.width - pad * 2, box.height - pad * 2};
  init();
  return loc();
}

cv::Point2d Tile::relative(int x, int y) {
  const cv::Rect box(bbox.x + img_offset.x, bbox.y + img_offset.y,
                     bbox.width - img_offset.x * 2,
                     bbox.height - img_offset.y * 2);
  return {static_cast<double>(x - box.x) / static_cast<double>(box.width),
          static_cast<double>(y - box.y) / static_cast<double>(box.height)};
}

cv::Mat Tile::raster() {
  if (updated) {
    alpha_blend(bg, fg, rasterized);
    updated = false;
  }
  return rasterized;
}

Tile &Tile::fill(cv::Mat img) {
  updated = true;
  if (img.cols > bbox.width || img.rows > bbox.height) {
    // Scale down to fit, retain aspect ratio
    const auto ratio = std::min(bbox.width / static_cast<double>(img.cols),
                                bbox.height / static_cast<double>(img.rows));
    cv::resize(img, img, cv::Size(), ratio, ratio);
  } else if (img.cols < bbox.width && img.rows < bbox.height) {
    // Scale up to fit, retain aspect ratio
    const auto ratio = std::min(bbox.width / static_cast<double>(img.cols),
                                bbox.height / static_cast<double>(img.rows));
    cv::resize(img, img, cv::Size(), ratio, ratio);
  }
  // Calculate offset (pad)
  img_offset = {(bbox.width - img.cols) / 2, (bbox.height - img.rows) / 2};
  // Place to center of tile
  const cv::Rect roi(img_offset.x, img_offset.y, img.cols, img.rows);
  img.copyTo(bg(roi));
  return *this;
}

Tile &Tile::fill(cv::Scalar color, double x2, double y2) {
  return fill(color, 0, x2, 0, y2);
}

Tile &Tile::fill(cv::Scalar color, double x1, double x2, double y1, double y2) {
  updated = true;
  img_offset = {0, 0};
  x1 = clamp<double>(x1, 0, 1);
  x2 = clamp<double>(x2, 0, 1);
  y1 = clamp<double>(y1, 0, 1);
  y2 = clamp<double>(y2, 0, 1);
  const unsigned x = rint(x1 * bbox.width), y = rint(y1 * bbox.height),
                 w = rint((x2 - x1) * bbox.width),
                 h = rint((y2 - y1) * bbox.height);
  cv::rectangle(bg, cv::Rect(x, y, w, h), color, cv::FILLED);
  return *this;
}

Tile &Tile::text(std::string text, cv::Scalar color, double height,
                 double pad) {
  clamp<double>(height, 0, 1);
  return this->text(text, color, (unsigned)rint(height * bbox.height), pad);
}

Tile &Tile::text(std::string text, cv::Scalar color, unsigned height,
                 double pad) {
  updated = true;
  text_renderer.cb = nullptr;
  try {
    // Clear foreground
    fg = color;
    // Get the basis for text size
    const auto base_size =
        cv::getTextSize("BASE", cv::FONT_HERSHEY_DUPLEX, 1, 2, nullptr);
    // Get the HiDPI scaled size (factor = 2)
    const double fs = 2 * static_cast<double>(height) /
                      static_cast<double>(base_size.height),
                 ft = fs * 2;
    const auto text_size =
        cv::getTextSize(text, cv::FONT_HERSHEY_DUPLEX, fs, rint(ft), nullptr);
    // Actual padding in px, derived from percentage
    // 2p = 1 - h / (h + 2x) => x = (1 - 1/(1 -2p)) - h) / 2
    const int p = rint(double(text_size.height) * pad / (1 - 2 * pad));
    const cv::Size bleed_size =
        cv::Size(text_size.width + p * 2, text_size.height + p * 2);
    // Create canvas for text mask
    cv::Mat mask(bleed_size, CV_8UC1, cv::Scalar{0});
    cv::putText(mask, text, cv::Point(p, bleed_size.height - p),
                cv::FONT_HERSHEY_DUPLEX, fs, {color[3]}, ft);
    // Resize to fit
    const double scale =
        std::min((double)height / (double)bleed_size.height,
                 (double)bbox.width / (double)bleed_size.width);
    cv::resize(mask, mask, cv::Size(), scale, scale, cv::INTER_AREA);
    const cv::Size size = mask.size();
    const cv::Rect roi =
        cv::Rect((bbox.width - size.width) / 2, (bbox.height - size.height) / 2,
                 size.width, size.height);
    std::vector<cv::Mat> channels;
    cv::split(fg, channels);
    channels.back() = {0};
    mask.copyTo(channels.back()(roi));
    cv::merge(channels, fg);
  } catch (cv::Exception &e) {
    std::cerr << LOG_NAME "Failed to render text: " << e.what() << std::endl;
  }
  return *this;
}

Tile &Tile::text(std::string (*cb)(cv::Point2d), cv::Scalar color,
                 double height, double pad) {
  const auto t = cb(val);
  text(t, color, height, pad);
  text_renderer = {cb, color, height, pad};
  return *this;
}

bool Tile::handle(PointerEvent pos) {
  if (!pos.valid)
    return false;
  const bool inside = contains(pos);
  if (pos.is_updated(1) && pos.is_down(1)) {
    active = inside;
  } else if (pos.is_updated(1) && !pos.is_down(1)) {
    active = false;
  }
  if (active) {
    val = relative(pos.x, pos.y);
    if (text_renderer.cb != nullptr) {
      const auto cb = text_renderer.cb;
      const auto t = cb(val);
      text(t, text_renderer.color, text_renderer.height, text_renderer.pad);
      text_renderer.cb = cb;
    }
  }
  return active;
}

} // namespace graphics
