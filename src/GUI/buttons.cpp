#include "GUI.h"

graphics::Tile &GUI::back_btn(cv::Rect box, int pad) {
  auto back_btn = new graphics::Tile(box, pad);
  back_btn->style.normal.fill = color::red(0.3);
  back_btn->style.active.fill = color::red(0.6);
  return back_btn->as(graphics::TileMode::BUTTON)
      .text("BACK")
      .use([&](graphics::Tile &tile, bool state_change) {
        if (!state_change || tile.is_active())
          return;
        global::flag_term = global::flag_back = true;
      });
}
