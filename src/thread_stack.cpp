#include "threads.h"

namespace thread {

void stack(Threading::FlushingPipe<cv::Mat> &pipe_in,
           Threading::FlushingPipe<cv::Mat> &pipe_out, const size_t n) {
  size_t counter = 0;
  cv::Mat stack;
  while (1) {
    try {
      auto image = pipe_in.read();
      if (counter == 0) {
        stack = cv::Mat(image->clone(), CV_16UC4);
      } else {
        cv::add(stack, *image, stack, cv::noArray(), CV_16UC4);
      }
      if (++counter >= n) {
        cv::Mat result(stack.size(), CV_8UC4);
        double minVal, maxVal;
        cv::minMaxLoc(stack, &minVal, &maxVal, NULL, NULL);
        stack -= minVal;
        cv::convertScaleAbs(stack, result, 255.0 / (maxVal - minVal));
        pipe_out.write(result);
        counter = 0;
      }
    } catch (Threading::Closed &e) {
      break;
    }
  }
  std::cout << "[Thread::stack] terminated." << std::endl;
}

} // namespace thread