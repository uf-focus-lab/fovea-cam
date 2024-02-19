#include "outfile.h"
#include "GUI.h"
#include "global.h"

#include <graphics/canvas.h>

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

#undef LOG_NAME
#define LOG_NAME "[outfile] "

using namespace graphics;

namespace outfile {

std::string out_file_prefix = "19691231-115959-unknown-";

std::vector<Item *> items;

void init(std::string task) {
  time_t currentTime;
  time(&currentTime);                // Get current time
  auto tm = localtime(&currentTime); // Convert current time to local time
  std::stringstream ss;
  ss << std::put_time(tm, "%y%m%d-%H%M%S-") << task << "-";
  out_file_prefix = ss.str();
};

void conclude() {
  if (items.size() == 0)
    return;
  std::cerr << LOG_NAME << "Asking for files to keep." << std::endl;
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  // Prepare tiles for interaction
  const int w = fb.shape().width, pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4, line_h = btn_h;
  int y = 0;
  Tile title = Tile(cv::Rect{0, y, w, line_h}, pad);
  title.style.bg = color::mono(0);
  title.style.text.color = color::mono(0.8);
  title.style.text.weight = 2;
  title.style.text.align = Align::CL;
  title.tbox({0, 0.25, 1, 0.5}).text("Select files to keep:");
  std::vector<Tile *> tiles = {&title};
  y += line_h;
  for (auto item : items) {
    Tile tile = Tile(cv::Rect{0, y, w, line_h}, pad);
    tile.style.normal.fill = color::mono(0.0);
    tile.style.active.fill = color::mono(0.2);
    tile.style.text.align = Align::CL;
    tile //
        .as(TileMode::BUTTON)
        .tbox({0, 0.1, 1, 0.8})
        .use([&item](Tile &tile, bool) {
          item->keep = !item->keep;
          item->update(tile);
        });
    item->update(tile);
    tiles.push_back(&tile);
    y += line_h;
  }
  y = fb.shape().height - btn_h;
  bool concluded = false;
  Tile btn_confirm =
      GUI::back_btn({btn_w, y, btn_w * 2, btn_h}, pad, "CONFIRM");
  tiles.push_back(
      &btn_confirm.use([&concluded](Tile &tile, bool) { concluded = true; }));
  // Start event loop
  canvas.clear().show(tiles).apply(fb);
  while (!concluded) {
    const auto pos = fb.wait_pointer(true);
    for (auto tile : tiles)
      tile->handle(pos);
    canvas.show(tiles).apply(fb, &pos);
  }
  // Remove files that are not marked to keep
  for (auto item : items) {
    if (!item->keep)
      remove(item->path.c_str());
    delete item;
  }
  items.clear();
}

Item::Item(std::string name, bool keep) : keep(keep) {
  path = out_file_prefix + name;
  fs = new std::ofstream(path, std::ios::app);
  std::cerr << LOG_NAME << "Writing to " << path << std::endl;
};

Item::~Item() {
  fs->close();
  delete fs;
};

void Item::update(graphics::Tile &tile) {
  tile.style.text.color = keep ? color::cyan(0.6) : color::red(0.5);
  tile.text((keep ? "[+] " : "[-] ") + path);
}

} // namespace outfile
