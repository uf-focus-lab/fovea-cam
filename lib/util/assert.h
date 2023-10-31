#pragma once

#include <stdexcept>

class AssertionError : public std::runtime_error {
public:
  // Inherit the constructors from std::runtime_error
  using std::runtime_error::runtime_error;
};

#define ASSERT(COND, MESSAGE)                                                  \
  {                                                                            \
    if (!(COND))                                                               \
      throw AssertionError(MESSAGE);                                           \
  }

#define CATCH_ASSERT(STATEMENTS)                                               \
  catch (AssertionError & e) {                                                 \
    std::cerr << e.what() << std::endl;                                        \
    { STATEMENTS }                                                             \
  }
