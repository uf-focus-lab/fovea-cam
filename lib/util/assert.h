#include <stdexcept>

#define ASSERT(COND, MESSAGE)                                                  \
  {                                                                            \
    if (!(COND))                                                               \
      throw std::runtime_error(MESSAGE);         \
  }
