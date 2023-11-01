#include "context.h"

#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h"

namespace thread {

extern bool flag_exit;

extern struct Env { char *FRAMERATE; } env;

void capture(Spinnaker::CameraPtr &camera,
             Threading::FastIO<cv::Mat> &pipe_out);

void capture(Spinnaker::CameraPtr &camera,
             std::vector<Threading::FastIO<cv::Mat> *> pipes_out);

void stack(Threading::FastIO<cv::Mat> &pipe_in,
           Threading::FastIO<cv::Mat> &pipe_out, const size_t n);

void display(Threading::FastIO<cv::Mat> &pipe_tile_a,
             std::vector<Threading::FastIO<cv::Mat> *> pipe_tile_b);

void mems(USB::SerialDevice &device,
          Threading::FIFO<context::MEMS_Position> &pos_in,
          Threading::FastIO<context::MEMS_Position> &pos_out);

void aruco(Threading::FastIO<cv::Mat> &pipe_mat_in,
           Threading::FIFO<std::vector<context::ArUcoInfo>> &pipe_info_out);

void track_pid(Threading::FIFO<std::vector<context::ArUcoInfo>> &fovea_info_in,
               Threading::FIFO<context::MEMS_Position> &mems_pos_next,
               Threading::FastIO<context::MEMS_Position> &mems_pos_back);

} // namespace thread
