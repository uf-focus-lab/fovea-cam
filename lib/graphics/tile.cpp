#include "tile.h"
#include "alpha.h"
#include "shared.h"
#include "util/clamp.h"

#undef LOG_NAME
#define LOG_NAME "[graphics::Tile] "

namespace graphics {

bool Tile::contains(PointerEvent pos) {
  return pos.x >= bbox.x && pos.x < bbox.x + bbox.width && pos.y >= bbox.y &&
         pos.y < bbox.y + bbox.height;
}

TileStyle &Tile::sty() { return active ? style.active : style.normal; }

Tile::Tile(cv::Rect box, int pad, TileMode mode) : mode(mode) { loc(box, pad); }

Tile &Tile::as(TileMode mode) {
  this->mode = mode;
  render(true);
  return *this;
}

Tile &Tile::use(TileStyleSheet &style) {
  this->style = style;
  return *this;
}

Tile &Tile::use(TileStyleSheet &&style) {
  this->style = style;
  return *this;
}

Tile &Tile::use(std::function<void(Tile &, bool)> &&handler) {
  this->handler = handler;
  return *this;
}

Tile &Tile::use(std::function<void(Tile &, bool)> &handler) {
  this->handler = handler;
  return *this;
}

Tile &Tile::use(cv::Point2d &val) {
  this->val = val;
  render(true);
  return *this;
}

Tile &Tile::use(cv::Point2d &&val) {
  this->val = val;
  render(true);
  return *this;
}

bool Tile::is_active() { return active; }

Tile &Tile::loc(cv::Rect box, int pad) {
  bbox = box;
  cbox = {pad, pad, box.width - pad * 2, box.height - pad * 2};
  cv::Size content = {cbox.width, cbox.height};
  bg = cv::Mat(content, CV_8UC4, style.bg);
  img_offset = {0, 0};
  render(true);
  return *this;
}

cv::Point2d Tile::relative(int x, int y) {
  const cv::Point origin(bbox.x + cbox.x + img_offset.x,
                         bbox.y + cbox.y + img_offset.y);
  const cv::Size size(cbox.width - img_offset.x * 2,
                      cbox.height - img_offset.y * 2);
  return {static_cast<double>(x - origin.x) / static_cast<double>(size.width),
          static_cast<double>(y - origin.y) / static_cast<double>(size.height)};
}

std::shared_ptr<const TileReadOut> Tile::read(bool once) {
  if (auto_raster)
    raster();
  if (!readout || !once)
    return buffer;
  return nullptr;
}

bool Tile::raster() {
  if (updated) {
    cv::Mat mat = cv::Mat(cv::Size{bbox.width, bbox.height}, CV_8UC4,
                          color::mono(0.0, 0.0));
    if (sty().outline.weight > 0) {
      int &weight = sty().outline.weight, offset = weight / 2;
      cv::Rect rect = {cbox.x - offset, cbox.y - offset, cbox.width + weight,
                       cbox.height + weight};
      cv::rectangle(mat, rect, sty().outline.color, weight);
    }
    cv::Mat content = mat(cbox);
    alpha_blend(content, bg, content);
    if (!fg.empty())
      alpha_blend(content, fg, content);
    updated = false;
    // Switch to new buffer (should be really fast)
    this->buffer = std::make_shared<const TileReadOut>(TileReadOut{
        .mat = mat,
        .bbox = bbox,
    });
    readout = false;
    return true;
  }
  return false;
}

Tile &Tile::fill(cv::Mat img) {
  if (img.cols > cbox.width || img.rows > cbox.height) {
    // Scale down to fit, retain aspect ratio
    const auto ratio = std::min(
        static_cast<double>(cbox.width) / static_cast<double>(img.cols),
        static_cast<double>(cbox.height) / static_cast<double>(img.rows));
    cv::resize(img, img, cv::Size(), ratio, ratio);
  } else if (img.cols < cbox.width && img.rows < cbox.height) {
    // Scale up to fit, retain aspect ratio
    const auto ratio = std::min(
        static_cast<double>(cbox.width) / static_cast<double>(img.cols),
        static_cast<double>(cbox.height) / static_cast<double>(img.rows));
    cv::resize(img, img, cv::Size(), ratio, ratio);
  }
  // Calculate offset (pad)
  img_offset = {(cbox.width - img.cols) / 2, (cbox.height - img.rows) / 2};
  // Place to center of tile
  const cv::Rect roi(img_offset.x, img_offset.y, img.cols, img.rows);
  bg = style.bg;
  img.copyTo(bg(roi));
  updated = true;
  return *this;
}

Tile &Tile::wipe() {
  bg = style.bg;
  updated = true;
  return *this;
}

Tile &Tile::fill(double x2, double y2) { return fill(0, x2, 0, y2); }

Tile &Tile::fill(double x1, double x2, double y1, double y2) {
  img_offset = {0, 0};
  x1 = clamp<double>(x1, 0, 1);
  x2 = clamp<double>(x2, 0, 1);
  y1 = clamp<double>(y1, 0, 1);
  y2 = clamp<double>(y2, 0, 1);
  const unsigned x = rint(x1 * cbox.width), y = rint(y1 * cbox.height),
                 w = rint((x2 - x1) * cbox.width),
                 h = rint((y2 - y1) * cbox.height);
  cv::rectangle(bg, cv::Rect(x, y, w, h), sty().fill, cv::FILLED);
  updated = true;
  return *this;
}

Tile &Tile::text(std::string text) {
  if (text.empty()) {
    fg = cv::Mat();
    return *this;
  }
  try {
    // Extract parameters
    const auto &weight = style.text.weight;
    const auto &height = static_cast<int>(
        rint(static_cast<double>(cbox.height) * style.text.height));
    const auto pad = style.text.inset;
    const auto color = style.text.color;
    // Get the basis for text size
    const auto base_size =
        cv::getTextSize(text, cv::FONT_HERSHEY_DUPLEX, 1, 2, nullptr);
    // Get the HiDPI scaled size (factor = 2)
    const double fs = 2 * static_cast<double>(height) /
                      static_cast<double>(base_size.height),
                 ft = fs * weight;
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
                 (double)cbox.width / (double)bleed_size.width);
    cv::resize(mask, mask, cv::Size(), scale, scale, cv::INTER_AREA);
    const cv::Size size = mask.size();
    const cv::Rect roi =
        cv::Rect((cbox.width - size.width) / 2, (cbox.height - size.height) / 2,
                 size.width, size.height);
    cv::Mat alpha = cv::Mat(cbox.height, cbox.width, CV_8UC1, cv::Scalar{0});
    mask.copyTo(alpha(roi));
    cv::merge(
        std::vector<cv::Mat>{
            cv::Mat(cbox.height, cbox.width, CV_8UC1, color[0]),
            cv::Mat(cbox.height, cbox.width, CV_8UC1, color[1]),
            cv::Mat(cbox.height, cbox.width, CV_8UC1, color[2]),
            alpha,
        },
        fg);
    updated = true;
  } catch (std::exception &e) {
    std::cerr << LOG_NAME "Failed to render text: " << e.what() << std::endl;
  }
  return *this;
}

bool Tile::handle(PointerEvent pos) {
  if (!pos.valid)
    return false;
  const bool _active = active;
  if (pos.is_updated(1) && pos.is_down(1))
    active = contains(pos);
  else if (pos.is_updated(1) && !pos.is_down(1))
    active = false;
  if (active)
    val = relative(pos.x, pos.y);
  bool state_change = _active != active;
  if (state_change)
    updated = true;
  return render(false, state_change);
}

bool Tile::render(bool force, bool state_change) {
  if (mode == TileMode::GENERIC) {
    handler(*this, state_change);
  } else if (mode == TileMode::X_SLIDER) {
    wipe().fill(val.x);
    handler(*this, state_change);
  } else if (mode == TileMode::Y_SLIDER) {
    wipe().fill(1.0, val.y);
    handler(*this, state_change);
  } else if (mode == TileMode::BUTTON && (state_change || force)) {
    fill();
    if (state_change && !active   //
        && val.x > 0 && val.y > 0 //
        && val.x < 1 && val.y < 1)
      handler(*this, state_change);
    else
      return false;
  } else {
    return false;
  }
  return true;
}

} // namespace graphics
