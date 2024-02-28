#pragma once
#include <fstream>
#include <string>
#include <vector>

#include <graphics/tile.h>

namespace outfile {

typedef struct FrameRecord {
  union {
    struct {
      uint8_t type;            // 1 Bytes
      uint8_t tag;             // 1 Byte
      uint16_t w, h;           // 4 Bytes
      uint8_t bytes_per_pixel; // 1 Byte
      uint64_t total_bytes;    // 8 Bytes
      uint64_t timestamp;      // 8 Bytes
    };
    uint8_t header[32];
  };
  uint8_t data[0];
} FrameRecord;

class Item {
public:
  Item(std::string name, std::ios::openmode mode = std::ios::app);
  ~Item();
  std::string name;
  std::ofstream *fs;
  std::string tmp_path();
  std::string final_path();
  bool keep = true;
  void update(graphics::Tile &tile);
};

extern std::string prefix;
extern std::vector<Item *> items;

void init(std::string task);
void conclude();

} // namespace outfile
