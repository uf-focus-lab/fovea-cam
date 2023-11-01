#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "util/assert.h"
#include <opencv2/opencv.hpp>
#include <opencv2/aruco.hpp>

namespace thread {

#undef LOGNAME
#define LOGNAME "thread::aruco"

void generateArucoMarker(){
  cv::Mat markerImage;
  cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
  cv::aruco::drawMarker(dictionary, 23, 200, markerImage, 1);
  cv::imwrite("/home/brevin/FoveaCam/marker23.png", markerImage);
}

bool save_test = true;

void aruco(Threading::FastIO<cv::Mat> &pipe_mat_in,
           Threading::FIFO<std::vector<context::ArUcoInfo>>& pipe_info_out) {
  try {
    std::shared_ptr<const cv::Mat> prev_ptr = nullptr;
    while (!flag_exit) {
      // generate aruco marker if doesnt exist
      // generateArucoMarker();
      // Read next frame from pipe
      auto next_ptr = pipe_mat_in.read();
      if (next_ptr == nullptr)
        continue; // No Data Available
      else if (next_ptr == prev_ptr)
        continue; // Same Data
      else {
        prev_ptr = next_ptr;
        cv::Mat img_test = next_ptr->clone();
        cv::Mat img_test_gray;
        cv::cvtColor(img_test, img_test_gray, cv::COLOR_RGBA2GRAY);
        //cv::imwrite("/home/brevin/FoveaCam/test.png", img_test_gray);
        cv::flip(img_test_gray, img_test_gray, 1);

        // The list of all detected markers
        std::vector<context::ArUcoInfo> info;
        // Do the detection
        std::vector<int> ids;
        std::vector<std::vector<cv::Point2f>> corners;
        cv::Ptr<cv::aruco::Dictionary> dictionary = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_250);
        cv::aruco::detectMarkers(img_test_gray, dictionary, corners, ids);
        
        // if at least one marker detected
        if (ids.size() > 0){
          float x_center = 0, y_center = 0;
          for (const auto& point : corners[0]) {
              x_center += point.x;
              y_center += point.y;
          }
          x_center /= 4;
          y_center /= 4;

          // todo: update to handle more than one marker
          info[0].x = x_center;
          info[0].y = y_center;
          info[0].id = 0;

          // Do something with the center, e.g., draw a circle at the center
          cv::circle(img_test_gray, cv::Point2f(x_center, y_center), 4, cv::Scalar(0, 255, 0), -1);
            cv::aruco::drawDetectedMarkers(img_test_gray, corners, ids);
            std::cout << LOGNAME "num markers: " << ids.size() << std::endl;
            std::cout << LOGNAME "num corners: " << corners[0].size() << std::endl; 


        }
        else{
          std::cout << LOGNAME "No marker detected" << std::endl;
        }
        // if(save_test)
        //   cv::imwrite("/home/brevin/FoveaCam/test.png", img_test_gray);
        // save_test = false;
        // cv::imshow("out", img_test_gray);
        // cv::waitKey(1);
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