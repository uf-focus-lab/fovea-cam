#include "tile.h"
#include "alpha.h"
#include "shared.h"
#include "util/assert.h"
#include "util/clamp.h"

#include <cmath>
#include <exception>
#include <opencv2/imgproc.hpp>
#include <sys/stat.h>

#undef LOG_NAME
#define LOG_NAME "[graphics::Tile] "

bool ratio_check(cv::MatSize _a, double r, double err = 1e-2) {
  cv::Size a = {_a[1], _a[0]};
  return std::abs(a.aspectRatio() - r) < err;
}

void fit(cv::Mat &m, cv::Size s) {
  if (m.empty())
    return;
  try {
    if (m.cols > s.width || m.rows > s.height) {
      // Scale down to fit, retain aspect ratio
      const auto ratio =
          std::min(static_cast<double>(s.width) / static_cast<double>(m.cols),
                   static_cast<double>(s.height) / static_cast<double>(m.rows));
      cv::resize(m, m, cv::Size(), ratio, ratio);
    } else if (m.cols < s.width && m.rows < s.height) {
      // Scale up to fit, retain aspect ratio
      const auto ratio =
          std::min(static_cast<double>(s.width) / static_cast<double>(m.cols),
                   static_cast<double>(s.height) / static_cast<double>(m.rows));
      cv::resize(m, m, cv::Size(), ratio, ratio, cv::INTER_NEAREST);
    }
  } catch (std::exception &e) {
    std::cerr << LOG_NAME "Failed to fit mat " << m.size << " into " << s
              << ": " << e.what() << std::endl;
  }
}

int r(double x) { return static_cast<int>(std::round(x)); }

namespace graphics {

bool Tile::contains(PointerEvent pos) {
  return pos.x >= bbox.x && pos.x < bbox.x + bbox.width && pos.y >= bbox.y &&
         pos.y < bbox.y + bbox.height;
}

TileStyle &Tile::sty() { return active ? style.active : style.normal; }

cv::Rect Tile::absolute(const cv::Rect2d &loc) {
  return cv::Rect{
      r(cbox.x + cbox.width * loc.x),
      r(cbox.y + cbox.height * loc.y),
      r(cbox.width * loc.width),
      r(cbox.height * loc.height),
  };
};

cv::Rect Tile::absolute(const cv::Rect2d &loc, cv::Mat &mat) {
  cv::Rect box = absolute(loc);
  fit(mat, box.size());
  if (mat.cols < box.width) {
    box.x += (box.width - mat.cols) / 2;
    box.width = mat.cols;
  }
  if (mat.rows < box.height) {
    box.y += (box.height - mat.rows) / 2;
    box.height = mat.rows;
  }
  ASSERT(mat.cols == box.width && mat.rows == box.height,
         LOG_NAME "mat size mismatch: " << mat.size() << " vs " << box.size());
  return box;
};

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
  render(true);
  return *this;
}

Tile &Tile::mbox(cv::Rect2d mbox) {
  m_loc = mbox;
  fit(mg, mbox.size());
  return *this;
}

Tile &Tile::tbox(cv::Rect2d tbox) {
  t_loc = tbox;
  fit(fg, tbox.size());
  return *this;
}

cv::Point2d Tile::relative(int x, int y) {
  cv::Rect box = mg.empty() ? cbox : absolute(m_loc, mg);
  x -= bbox.x;
  y -= bbox.y;
  return {(x - box.x) / static_cast<double>(box.width),
          (y - box.y) / static_cast<double>(box.height)};
}

std::shared_ptr<const TileReadOut> Tile::read(bool once) {
  if (auto_raster)
    raster();
  if (!readout || !once)
    return buffer;
  return nullptr;
}

Tile& Tile::raster() {
  if (!updated)
    return *this;
  cv::Mat mat = cv::Mat(cv::Size{bbox.width, bbox.height}, CV_8UC4,
                        color::mono(0.0, 0.0));
  // 1. Render outline onto canvas
  if (sty().outline.weight > 0) {
    int &weight = sty().outline.weight, offset = weight / 2;
    cv::Rect rect = {cbox.x - offset, cbox.y - offset, cbox.width + weight,
                     cbox.height + weight};
    cv::rectangle(mat, rect, sty().outline.color, weight);
  }
  // 2. Blend bg onto canvas
  if (bg.empty())
    bg = cv::Mat(cbox.size(), CV_8UC4, style.bg);
  {
    const cv::Rect2d c_loc{0, 0, 1, 1};
    cv::Mat img = mat(absolute(c_loc, bg));
    alpha_blend(img, bg, img);
  }
  // 3. Blend mg onto canvas (optional)
  if (!mg.empty()) {
    cv::Mat img = mat(absolute(m_loc, mg));
    alpha_blend(img, mg, img);
  }
  // 4. Blend fg onto canvas (optional)
  if (!fg.empty()) {
    cv::Mat text = mat(absolute(t_loc, fg));
    alpha_blend(text, fg, text);
  }
  // last, unset updated flag
  updated = false;
  // finally, switch to new buffer (should be really fast)
  this->buffer = std::make_shared<const TileReadOut>(TileReadOut{
      .mat = mat,
      .bbox = bbox,
  });
  // and reset readout flag
  readout = false;
  return *this;
}

Tile &Tile::wipe() {
  bg = cv::Mat();
  updated = true;
  return *this;
}

Tile &Tile::fill(double x2, double y2) { return fill(0, x2, 0, y2); }

Tile &Tile::fill(double x1, double x2, double y1, double y2) {
  x1 = clamp<double>(x1, 0, 1);
  x2 = clamp<double>(x2, 0, 1);
  y1 = clamp<double>(y1, 0, 1);
  y2 = clamp<double>(y2, 0, 1);
  const unsigned x = r(x1 * cbox.width), y = r(y1 * cbox.height),
                 w = r((x2 - x1) * cbox.width), h = r((y2 - y1) * cbox.height);
  if (bg.empty())
    bg = cv::Mat(cbox.size(), CV_8UC4, style.bg);
  cv::rectangle(bg, cv::Rect(x, y, w, h), sty().fill, cv::FILLED);
  updated = true;
  return *this;
}

Tile &Tile::use(cv::Mat img) {
  if (img.empty()) {
    mg = cv::Mat();
  } else {
    mg = img.clone();
    fit(mg, absolute(m_loc).size());
  }
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
    const auto tbox = absolute(t_loc);
    const auto &width = tbox.width;
    const auto &height = tbox.height;
    const auto &weight = style.text.weight;
    const auto inset = height * style.text.inset;
    const auto color = style.text.color;
    const double R = 2.0;
    const auto FONT = cv::FONT_HERSHEY_DUPLEX;
    // Get the basis for text size
    const auto base_size = cv::getTextSize(text, FONT, 1, weight, nullptr);
    // Get the HiDPI scaled size (factor = R)
    const double fs = R * std::min((height - 2 * inset) / base_size.height,
                                   (width - 2 * inset) / base_size.width),
                 ft = fs * weight;
    const auto inner = cv::getTextSize(text, FONT, fs, r(ft), nullptr);
    const auto outer = inner + cv::Size{r(inset * R), r(inset * R)} * 2;
    const cv::Point offset = {r(inset * R), outer.height - r(inset * R)};
    // Create canvas for text mask
    cv::Mat mask(outer, CV_8UC1, cv::Scalar{0});
    cv::putText(mask, text, offset, FONT, fs, {color[3]}, ft);
    cv::merge(
        std::vector<cv::Mat>{
            cv::Mat(outer, CV_8UC1, color[0]),
            cv::Mat(outer, CV_8UC1, color[1]),
            cv::Mat(outer, CV_8UC1, color[2]),
            mask,
        },
        fg);
    fit(fg, tbox.size());
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
    handler(*this, state_change);
    wipe().fill(val.x);
  } else if (mode == TileMode::Y_SLIDER) {
    handler(*this, state_change);
    wipe().fill(1.0, val.y);
  } else if (mode == TileMode::BUTTON && (state_change || force)) {
    fill();
    if (state_change && !active       //
        && val.x > 0.0 && val.y > 0.0 //
        && val.x < 1.0 && val.y < 1.0)
      handler(*this, state_change);
    else
      return false;
  } else {
    return false;
  }
  return true;
}

} // namespace graphics
