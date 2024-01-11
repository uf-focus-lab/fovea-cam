#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/dictionary.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

namespace threads {

#undef LOGNAME
#define LOGNAME "[threads::aruco]"

unsigned counter = 0;

void aruco(MatPipe &pipe_mat_in,
           ArUcoPipe &pipe_info_out,
           bool transform) {
  try {
    std::shared_ptr<const cv::Mat> prev_ptr = nullptr;
    while (!global::flag_term) {
      // Read next frame from pipe
      auto next_ptr = pipe_mat_in.read();
      if (next_ptr == nullptr)
        continue; // No Data Available
      else if (next_ptr == prev_ptr)
        continue; // Same Data
      prev_ptr = next_ptr;
      // Process new frame
      cv::Mat mat, global;
      cv::cvtColor(*next_ptr, mat, cv::COLOR_RGBA2GRAY);
      if (transform) {
        // Resize to 1/2
        cv::resize(mat, mat, cv::Size(), 0.5, 0.5);
        // extend to float 32
        mat.convertTo(mat, CV_32FC1);
        cv::Mat global;
        cv::GaussianBlur(mat, mat, cv::Size(3, 3), 0, 0);
        cv::GaussianBlur(mat, global, cv::Size(99, 99), 0, 0);
        // Run threshold according to global light
        cv::subtract(mat, global, mat);
        cv::threshold(mat, mat, 0, 255, cv::THRESH_BINARY);
        mat.convertTo(mat, CV_8UC1);
      }
      // The list of all detected markers
      std::vector<context::ArUcoInfo> info;
      // Do the detection
      std::vector<int> ids;
      std::vector<std::vector<cv::Point2f>> corners;
      cv::Ptr<cv::aruco::Dictionary> dictionary =
          cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_100);
      cv::aruco::detectMarkers(mat, dictionary, corners, ids);
      // if at least one marker detected
      const double x_center = (double)mat.cols / 2,
                   y_center = (double)mat.rows / 2;
      for (unsigned i = 0; i < ids.size(); i++) {
        const int id = ids[i];
        const auto c = corners[i];
        context::ArUcoInfo marker_info = {.id = id};
        if (transform) {
          for (const auto &point : c) {
            marker_info.corners.push_back(cv::Point2f(
                2 * (point.x - x_center), 2 * (point.y - y_center)));
          }
        } else {
          for (const auto &point : c) {
            marker_info.corners.push_back(
                cv::Point2f(point.x - x_center, point.y - y_center));
          }
        }
        // todo: update to handle more than one marker
        info.push_back(marker_info);
      }
      pipe_info_out.write(info);
    }
  } catch (Threading::END &e) {
    std::cerr << LOGNAME " PIPE END" << std::endl;
  } catch (std::exception &e) {
    std::cerr << LOGNAME "  " << e.what() << std::endl;
  }
  pipe_mat_in.close();
  pipe_info_out.close();
  std::cerr << LOGNAME " terminated." << std::endl;
}

} // namespace thread
