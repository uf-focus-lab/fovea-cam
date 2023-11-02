#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"
#include <opencv2/aruco.hpp>
#include <opencv2/aruco/dictionary.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

namespace thread {

#undef LOGNAME
#define LOGNAME "[thread::aruco]"

void generateArucoMarker() {
  cv::Mat markerImage;
  cv::Ptr<cv::aruco::Dictionary> dictionary =
      cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
  cv::aruco::drawMarker(dictionary, 23, 200, markerImage, 1);
  cv::imwrite("./marker23.png", markerImage);
}

unsigned counter = 0;

void aruco(Threading::FastIO<cv::Mat> &pipe_mat_in,
           Threading::FIFO<std::vector<context::ArUcoInfo>> &pipe_info_out) {
  try {
    std::shared_ptr<const cv::Mat> prev_ptr = nullptr;
    while (!flag_exit) {
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
      // Resize to 1/2
      cv::resize(mat, mat, cv::Size(), 0.5, 0.5);
      // extend to float 32
      mat.convertTo(mat, CV_32FC1);
      cv::GaussianBlur(mat, mat, cv::Size(3, 3), 0, 0);
      cv::GaussianBlur(mat, global, cv::Size(99, 99), 0, 0);
      // Run threshold according to global light
      cv::subtract(mat, global, mat);
      cv::threshold(mat, mat, 0, 255, cv::THRESH_BINARY);
      mat.convertTo(mat, CV_8UC1);
      // The list of all detected markers
      std::vector<context::ArUcoInfo> info;
      // Do the detection
      std::vector<int> ids;
      std::vector<std::vector<cv::Point2f>> corners;
      cv::Ptr<cv::aruco::Dictionary> dictionary =
          cv::aruco::getPredefinedDictionary(cv::aruco::DICT_4X4_100);
      cv::aruco::detectMarkers(mat, dictionary, corners, ids);
      // if at least one marker detected
      if (ids.size() > 0) {
        float x_center = 0, y_center = 0;
        for (const auto &point : corners[0]) {
          x_center += point.x;
          y_center += point.y;
        }
        x_center /= 4;
        y_center /= 4;

        // draw center
        // cv::circle(mat, cv::Point2f(x_center, y_center), 4,
        // cv::Scalar(0, 255, 0), -1);

        // Making the coordinates relative to the center of the image
        x_center -= (double)mat.cols / 2;
        y_center -= (double)mat.rows / 2;

        // todo: update to handle more than one marker
        info.push_back({0, -x_center, y_center});
        // Do something with the center, e.g., draw a circle at the center
        // cv::aruco::drawDetectedMarkers(mat, corners, ids);
        // std::cout << LOGNAME " num markers: " << ids.size() << std::endl;
        // std::cout << LOGNAME " num corners: " << corners[0].size() <<
        // std::endl;
        std::cout << LOGNAME " Detected <" << ids[0] << ">" << std::endl;
        pipe_info_out.write(info);
      } else {
        std::cout << LOGNAME " No marker detected" << std::endl;
      }
      // Sort by id (ascending)
      // Deduplicate (id should be unique)
      // Send to output pipe
    }
  } catch (Threading::END &e) {
  } catch (std::exception &e) {
    std::cerr << LOGNAME "  " << e.what() << std::endl;
  }
  pipe_mat_in.close();
  pipe_info_out.close();
  std::cout << LOGNAME " terminated." << std::endl;
}

} // namespace thread
