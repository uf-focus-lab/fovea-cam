#include "args.h"
#include <argparse/argparse.hpp>
#include <cstdlib>

argparse::ArgumentParser parser("FoveaCam");

namespace args {

void parse(int argc, const char *const argv[]) {

  parser.add_argument("action").default_value("view").help(
      "display the square of a given integer");

  parser.add_argument("-d", "--display")
      .default_value(std::string{"/dev/fb0"})
      .help("path to display's framebuffer device");

  parser.add_argument("-s", "--stack")
      .default_value(0)
      .help("number of frames to stack from zoom camera")
      .scan<'i', int>();

  parser.add_argument("--mems-serial")
      .help("path to the serial port of MEMS driver");

  parser.add_argument("--lens-serial")
      .help("path to the serial port of zoom lens driver");

  try {
    parser.parse_args(argc, argv);
  } catch (const std::runtime_error &err) {
    std::cerr << err.what() << std::endl;
    std::cerr << parser;
    std::exit(-1);
  }
}

} // namespace args
