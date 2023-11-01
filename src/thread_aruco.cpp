#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"

namespace thread {

#undef LOGNAME
#define LOGNAME "thread::aruco"

void aruco(Threading::FastIO<cv::Mat> &pipe_mat_in,
           Threading::FIFO<std::vector<context::ArUcoInfo>> pipe_info_out) {
  try {
    std::shared_ptr<const cv::Mat> prev_ptr = nullptr;
    while (!flag_exit) {
      // Read next frame from pipe
      auto next_ptr = pipe_mat_in.read();
      if (next_ptr == nullptr)
        continue; // No Data Available
      else if (next_ptr == prev_ptr)
        continue; // Same Data
      else {
        // The list of all detected markers
        std::vector<context::ArUcoInfo> info;
        // Do the detection
        // Sort by id (ascending)
        // Deduplicate (id should be unique)
        // Send to output pipe
        pipe_info_out.write(info);
      }
    };
  } catch (Threading::END) {
  } catch (std::exception &e) {
    std::cerr << LOGNAME " " << e.what() << std::endl;
  }
  pipe_mat_in.close();
  pipe_info_out.close();
  std::cout << LOGNAME " terminated." << std::endl;
}

} // namespace thread