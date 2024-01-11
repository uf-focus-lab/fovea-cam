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

#define CATCH_ASSERT(LOGNAME)                                                  \
  catch (AssertionError & e) {                                                 \
    std::cerr << "[ASSERTION ERROR] " << e.what() << std::endl;                \
  }                                                                            \
  catch (...) {                                                                \
    std::cerr << LOGNAME "Unknown Error" << std::endl;                         \
  }
