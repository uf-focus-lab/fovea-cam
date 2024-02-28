#include "outfile.h"
#include "GUI.h"
#include "global.h"
#include "graphics/tile.h"

#include <graphics/canvas.h>

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

#undef LOGNAME
#define LOGNAME "[outfile] "

using namespace graphics;

namespace outfile {

std::string prefix = "19691231-115959-unknown-task";

std::vector<Item *> items;

void init(std::string task) {
  time_t currentTime;
  time(&currentTime);                // Get current time
  auto tm = localtime(&currentTime); // Convert current time to local time
  std::stringstream ss;
  ss << std::put_time(tm, "%y%m%d-%H%M%S-") << task;
  prefix = ss.str();
};

void conclude() {
  if (items.size() == 0) {
    std::cerr << LOGNAME << "Nothing to confirm." << std::endl;
    return;
  }
  std::cerr << LOGNAME << "Asking for files to keep." << std::endl;
  auto &fb = *global::fb;
  Canvas canvas(fb.shape());
  // Prepare tiles for interaction
  const int w = fb.shape().width, pad = w / 64;
  const int btn_h = w / 8, btn_w = w / 4, line_h = btn_h,
            footnote_h = line_h / 2, space_h = 4 + 2 * pad;
  bool concluded = false;
  const int list_top = line_h + space_h,
            list_end = fb.shape().height - footnote_h - btn_h - line_h;
  if (list_top >= list_end) {
    std::cerr << LOGNAME << "No space on screen to show list options"
              << std::endl;
    return;
  }
  // Organize pages
  unsigned int page = 0;
  std::vector<std::vector<Tile *>> pages = {};
  // Flag to force rerender everything
  bool flag_rerender = true;
  // Title
  int x = 0, y = 0;
  Tile title({pad, y, w - 2 * pad, line_h}, pad);
  title.style.bg = color::mono(0);
  title.style.text.color = color::mono(0.8);
  title.style.text.weight = 2;
  title.style.text.align = Align::CL;
  title.as(TileMode::INACTIVE)
      .tbox({0, 0.4, 1, 0.6})
      .text("Select files to keep:");
  // H line
  Tile spacer({pad, line_h, w - 2 * pad, space_h}, pad);
  spacer.style.normal.fill = color::mono(0.8);
  spacer.as(TileMode::INACTIVE).fill();
  // Footnote
  y = fb.shape().height - footnote_h - btn_h;
  Tile footnote({pad, y, w - 2 * pad, footnote_h}, pad);
  footnote.style.normal.fill = color::mono(0);
  footnote.style.text.weight = 1.2;
  footnote.style.text.color = color::mono(0.25);
  footnote.as(TileMode::INACTIVE)
      .tbox({0, 0, 1, 1})
      .text("Selected files will be saved to " + prefix)
      .fill();
  // Buttons (optional)
  y = fb.shape().height - btn_h;
  Tile btn_prev({x, y, btn_w, btn_h}, pad);
  btn_prev.style.bg = color::mono(0);
  btn_prev.style.active.fill = color::mono(0.2);
  btn_prev.style.text.weight = 4;
  btn_prev.as(TileMode::BUTTON)
      .text("<")
      .use([&flag_rerender, &canvas, &page](Tile &tile, bool) {
        if (page > 0)
          page--;
        canvas.clear();
        flag_rerender = true;
      });
  x += btn_w;
  Tile btn_confirm =
      GUI::back_btn({btn_w, y, btn_w * 2, btn_h}, pad, "CONFIRM")
          .use([&concluded](Tile &tile, bool) { concluded = true; });
  x += 2 * btn_w;
  Tile btn_next({x, y, btn_w, btn_h}, pad);
  btn_next.style.bg = color::mono(0);
  btn_next.style.active.fill = color::mono(0.2);
  btn_next.style.text.weight = 4;
  btn_next.as(TileMode::BUTTON)
      .text(">")
      .use([&flag_rerender, &canvas, &pages, &page](Tile &tile, bool) {
        if (page + 1 < pages.size())
          page++;
        canvas.clear();
        flag_rerender = true;
      });
  pages.push_back({&title, &spacer, &footnote, &btn_confirm});
  y = list_top;
  std::vector<Tile *> list_items = {};
  for (auto item : items) {
    auto *tiles = &pages.back();
    if (y > list_end) {
      tiles->push_back(&btn_next);
      pages.push_back({&title, &spacer, &footnote, &btn_prev, &btn_confirm});
      tiles = &pages.back();
      y = list_top;
    }
    auto tile = new Tile({pad, y, w - 2 * pad, line_h}, pad);
    tile->style.text.align = Align::CL;
    tile->style.text.weight = 1.5;
    tile->style.normal.outline.weight = tile->style.active.outline.weight;
    (*tile) //
        .as(TileMode::BUTTON)
        .tbox({0.01, 0.25, 0.98, 0.5})
        .use([item](Tile &tile, bool) {
          item->keep = !item->keep;
          item->update(tile);
        });
    item->update(*tile);
    // std::cerr << LOGNAME << "Updated list item " << item->name << " @ "
    //           << tile.read(false)->mat.size << std::endl;
    tiles->push_back(tile);
    list_items.push_back(tile);
    y += line_h;
  }
  // Start event loop
  canvas.clear().show(pages[page]).apply(fb);
  while (!concluded) {
    const auto pos = fb.wait_pointer();
    for (auto tile : pages[page])
      tile->handle(pos);
    canvas.show(pages[page], flag_rerender).apply(fb, &pos);
    flag_rerender = false;
  }
  // Release manually allocated tiles
  for (auto tile : list_items)
    delete tile;
  // Remove files that are not marked to keep
  for (auto item : items) {
    delete item;
  }
  items.clear();
}

Item::Item(std::string name, std::ios::openmode mode) : name(name) {
  fs = new std::ofstream(tmp_path(), mode);
  std::cerr << LOGNAME << "Created " << name << std::endl;
};

std::string Item::tmp_path() { return "/tmp/" + prefix + "-" + name; }

std::string Item::final_path() { return prefix + "/" + name; }

Item::~Item() {
  // Deinitialize out file stream
  delete fs;
  // Move file to dedicated folder under PWD if user wants to keep it
  if (keep) {
    std::filesystem::create_directories(prefix);
    try {
      std::filesystem::copy(tmp_path(), final_path());
    } catch (std::filesystem::filesystem_error &e) {
      std::cerr << LOGNAME << e.what() << std::endl;
    }
  }
};

void Item::update(graphics::Tile &tile) {
  tile.style.normal.outline.color = keep ? color::cyan(0.6) : color::mono(0.2);
  tile.style.active.outline.color = keep ? color::cyan(0.8) : color::mono(0.6);
  tile.style.bg = keep ? color::cyan(0.1) : color::mono(0.0);
  tile.style.active.fill = keep ? color::cyan(0.2) : color::mono(0.2);
  tile.bg.release();
  tile.style.text.color = keep ? color::mono(0.8) : color::mono(0.6);
  tile.text((keep ? "[+] " : "[-] ") + name);
}

} // namespace outfile
