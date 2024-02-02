#pragma once

#include <iomanip>
#include <sstream>
#include <string>

static inline std::string fmt(double val, unsigned n, unsigned p, char fill = ' ') {
  char sign = val >= 0 ? '+' : '-';
  val = std::abs(val);
  std::stringstream ss;
  ss << sign << std::fixed << std::setprecision(p) << std::setw(n + p + 1)
     << std::setfill(fill) << val;
  return ss.str();
}
