#include <string>

namespace args {

std::string action;

std::string framebuffer_path;

unsigned int stack;

std::string mems_serial;

std::string lens_serial;

void parse(int argc, const char *const argv[]);

}
