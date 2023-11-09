#include <fstream>
#include <glob.h>
#include <iostream>
#include <string>
#include <sys/signal.h>
#include <vector>

namespace vtconsole {

static const char PATTERN[] = "/sys/class/vtconsole/vtcon*";

static std::vector<std::string> restore_list;

int write(std::string path, std::string value) {
  std::ofstream unbind_file(path);
  if (unbind_file.is_open()) {
    unbind_file << value;
    return 0;
  }
  return -1;
}

void restore() {
  for (const auto &path : restore_list) {
    write(path + "/bind", "1");
    std::cerr << "[vtconsole::restore] " << path << std::endl;
  }
}

void unbind_all(bool restore_on_exit) {
  if (restore_on_exit)
    atexit(restore);
  glob_t glob_result;
  glob(PATTERN, GLOB_ONLYDIR, NULL, &glob_result);
  for (unsigned int i = 0; i < glob_result.gl_pathc; ++i) {
    // Check if this vtconsole is a frame buffer device
    auto const path = std::string(glob_result.gl_pathv[i]);
    std::ifstream vtcon_name(path + "/name"), vtcon_bind(path + "/bind");
    if (vtcon_name.is_open() && vtcon_bind.is_open()) {
      std::string name, bind;
      getline(vtcon_name, name);
      getline(vtcon_bind, bind);
      if (name.substr(0, 3) != "(M)")
        continue;
      // Check for current bind state
      if (bind.substr(0, 1) != "1")
        continue;
      // Unbind and add to restore_list
      if (write(path + "/bind", "0") == 0) {
        restore_list.push_back(path);
        std::cerr << "[vtconsole::unbind] " << path << std::endl;
      } else {
        std::cerr << "[vtconsole::WARNING] Failed to unbind " << path
                  << std::endl;
      }
    }
  }
  globfree(&glob_result);
}

} // namespace vtconsole
