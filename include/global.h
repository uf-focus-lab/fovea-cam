#pragma once

#include <exception>

#include "context.h"
#include "threads.h"

#include "mems/mems.h"

#define NO_THROW(STATEMENT)                                                    \
  try {                                                                        \
    STATEMENT;                                                                 \
  } catch (std::exception & e) {                                               \
    std::cerr << "[" << __func__ << "] " << e.what() << std::endl;             \
  } catch (...) {                                                              \
  }

namespace global {
extern Threading::FastIO<cv::Mat> wide_capture_pipe;
extern std::vector<Threading::FastIO<cv::Mat> *> fovea_pipes;
extern Threading::FIFO<mems::Position> pos_next;
extern Threading::FastIO<mems::Position> pos_back;
extern Threading::FastIO<mems::Position> pos_real;
} // namespace global