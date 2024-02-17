#pragma once
#include <fstream>
#include <string>
#include <vector>

#include <graphics/tile.h>

namespace outfile {
class Item {
public:
  Item(std::string path, bool keep = true);
  ~Item();
  std::string path;
  std::ofstream *fs;
  bool keep;
  void update(graphics::Tile &tile);
};

extern std::string prefix;
extern std::vector<Item *> items;

void init(std::string task);
void conclude();

} // namespace outfile
