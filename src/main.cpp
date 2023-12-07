#include "context.h"
#include "threads.h"

#include "graphics/canvas.h"
#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h"
#include "util/vtconsole.h"

#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <signal.h>
#include <thread>
// Create pipes
Threading::FastIO<cv::Mat> wide_capture_pipe;
std::vector<Threading::FastIO<cv::Mat> *> fovea_pipes;
Threading::FIFO<context::MEMS_Position> pos_next(1);
Threading::FastIO<context::MEMS_Position> pos_back;

#define NO_THROW(STATEMENT)                                                    \
  try {                                                                        \
    STATEMENT;                                                                 \
  } catch (std::exception & e) {                                               \
    std::cerr << "[" << __func__ << "] " << e.what() << std::endl;             \
  } catch (...) {                                                              \
  }

void close_all_pipes(int) {
  std::cerr << std::endl;
  thread::flag_exit = true;
  NO_THROW(wide_capture_pipe.close());
  for (auto &pipe : fovea_pipes) {
    NO_THROW(pipe->close());
  }
  NO_THROW(pos_next.close());
  NO_THROW(pos_back.close());
  // Restore all signals to default
  signal(SIGINT, SIG_DFL);
  signal(SIGKILL, SIG_DFL);
  signal(SIGTERM, SIG_DFL);
}

int main(int argc, char **argv) {
  const auto task = argc > 1 ? std::string(argv[1]) : std::string{"move"};
  // Get env pointers
  thread::env.FRAMERATE = std::getenv("FPS");
  // Register signal handler
  signal(SIGINT, close_all_pipes);
  signal(SIGKILL, close_all_pipes);
  signal(SIGTERM, close_all_pipes);
  // Initialize serial port
  USB::SerialDevice mems(0x16c0, 0x0483);
  // Initialize cameras
  std::cerr << "[main] Looking for spinnaker cameras." << std::endl;
  auto spinnaker = Spinnaker::System::GetInstance();
  auto camList = spinnaker->GetCameras();
  Spinnaker::CameraPtr wide_camera(nullptr), fovea_camera(nullptr);
  for (unsigned idx = 0; idx < camList.GetSize(); idx++) {
    auto camera = camList[idx];
    camera->Init();
    const auto model = std::string(camera->DeviceModelName().c_str());
    if (model.ends_with("BFS-U3-16S2C")) {
      // wide angle camera
      wide_camera = camera;
    } else if (model.ends_with("BFS-U3-16S2C-BD")) {
      // fovea camera
      fovea_camera = camera;
    }
    camera->DeInit();
  }
  if (wide_camera == nullptr || fovea_camera == nullptr) {
    std::cerr << "[main] Unable to find cameras." << std::endl;
    camList.Clear();
    spinnaker->ReleaseInstance();
    return -1;
  }
  // Begin acquisition
  std::vector<std::thread> threads;
  // Task specific threads
  if (task == "move") {
    // Multiplex 4 streams
    for (unsigned i = 0; i < 4; i++) {
      fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
    }
    // Send positions in new thread
    threads.push_back(std::thread([&]() {
      try {
        // Broadcast idle position
        pos_next.write({0, 0});
        pos_next.write({0, 0});
        std::this_thread::sleep_for(std::chrono::seconds(1));
        double step = 1.0, pos = 0.0;
        while (true) {
          pos += step;
          if (pos >= 80.0)
            step = -1.0;
          if (step < 0.0 && pos <= 0.0)
            break;
          pos_next.write({pos, pos, 2});
          pos_next.write({-pos, pos, 1});
          pos_next.write({-pos, -pos, 3});
          pos_next.write({pos, -pos, 4});
        }
        // Broadcast idle position
        pos_next.write({0, 0});
        pos_next.write({0, 0});
        std::this_thread::sleep_for(std::chrono::seconds(1));
      } catch (Threading::END &) {
        // Normal termination
      }
      // Close position pipe upon fifo emptied
      NO_THROW(pos_next.close(true));
    }));
  } else if (task == "track") {
    // Create 1 stream for fovea
    fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
    // Create control pipes
    const auto
        // Wide angle -> aruco position
        wide_aruco_pos_pipe =
            new Threading::FastIO<std::vector<context::ArUcoInfo>>,
        // Fovea -> aruco position
        fovea_aruco_pos_pipe =
            new Threading::FastIO<std::vector<context::ArUcoInfo>>;
    // Aruco detection thread
    threads.push_back(std::thread([&]() {
      thread::aruco(wide_capture_pipe, *wide_aruco_pos_pipe, false);
    }));
    threads.push_back(std::thread([&]() {
      thread::aruco(*fovea_pipes[0], *fovea_aruco_pos_pipe, true);
    }));
    // Tracking thread
    threads.push_back(std::thread([&]() {
      thread::track_pid(*wide_aruco_pos_pipe, *fovea_aruco_pos_pipe, pos_next,
                        pos_back);
    }));
  } else if (task == "match") {
    // Create 2 streams for fovea view
    // First being the real fovea image
    fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
    // Second being the matched portion of wide angle image
    fovea_pipes.push_back(new Threading::FastIO<cv::Mat>);
    // Thread to move the mems in a circular pattern
    threads.push_back(std::thread([&]() {
      try {
        // Broadcast idle position
        pos_next.write({0, 0, 2});
        static const double r = 80.0;
        double theta = 0.0;
        while (true) {
          theta += 0.02;
          if (theta >= 2 * M_PI)
            theta -= 2 * M_PI;
          const double x = r * cos(theta), y = r * sin(theta);
          pos_next.write({x, y, 2});
        }
      } catch (Threading::END &) {
        // Normal termination
      }
      // Close position pipe upon fifo emptied
      NO_THROW(pos_next.close(true));
    }));
    // Thread to push matched image to 2nd fovea pipe
    threads.push_back(std::thread([&]() {
      try {
        auto prev_fovea = fovea_pipes[1]->read();
        while (true) {
          auto fovea = fovea_pipes[1]->read();
          if (fovea == prev_fovea || fovea == nullptr)
            continue;
          prev_fovea = fovea;
          // Get latest mems position
          auto pos = pos_back.read();
          if (pos == nullptr)
            continue;
          // Get latest image
          auto wide = wide_capture_pipe.read();
          if (wide == nullptr)
            continue;
          // Calculate cropping region
          static const double SCALE = 4.65;
          const auto V_x = pos->x, V_y = pos->y;
          const int W = wide->size().width, H = wide->size().height;
          const struct {
            double x, y, xy, c;
          } x_coeff = {4.85459339325059, -0.06452967035358549,
                       0.00016280733222939038, -63.032525040658314},
            y_coeff = {-0.13787596565316557, -3.435103383528066,
                       0.0017389896032312644, -64.05920615697792};
          const double P_x = x_coeff.x * V_x + x_coeff.y * V_y +
                             x_coeff.xy * V_x * V_y + x_coeff.c,
                       P_y = y_coeff.x * V_x + y_coeff.y * V_y +
                             y_coeff.xy * V_x * V_y + y_coeff.c;
          int x = (1.0 - 1.0 / x_coeff.x) * (double)(W) / 2.0 + P_x,
              y = (1.0 + 1.0 / y_coeff.y) * (double)(H) / 2.0 + P_y,
              w = (double)(W) / SCALE, h = (double)(H) / SCALE;
          if (x < 0)
            x = 0;
          if (x + w > W)
            x = W - w;
          if (y < 0)
            y = 0;
          if (y + h > H)
            y = H - h;
          cv::Rect roi = cv::Rect(x, y, w, h);
          // Push to pipe
          try {
            cv::Mat matched;
            cv::resize((*wide)(roi), matched, fovea->size());
            fovea_pipes[0]->write(std::move(matched));
          } catch (cv::Exception &e) {
            std::cerr << "[main] out of bound roi: " << roi << std::endl;
          }
        }
      } catch (Threading::END &) {
        // Normal termination
      }
      fovea_pipes[0]->close();
    }));
  } else {
    std::cerr << "[main] Unknown task: " << task << std::endl;
    return -1;
  }
  // Display Thread
  threads.push_back(
      std::thread([&]() { thread::display(wide_capture_pipe, fovea_pipes); }));
  // Capture Threads
  threads.push_back(
      std::thread([&]() { thread::capture(wide_camera, wide_capture_pipe); }));
  threads.push_back(
      std::thread([&]() { thread::capture(fovea_camera, fovea_pipes); }));
  // MEMS Thread
  threads.push_back(
      std::thread([&]() { thread::mems(mems, pos_next, pos_back); }));
  // Wait for threads to terminate
  for (auto &thread : threads) {
    thread.join();
  }
  // Release resources
  NO_THROW(camList.Clear());
  NO_THROW(spinnaker->ReleaseInstance());
  std::cerr << "[main] terminated." << std::endl;
  return 0;
}
