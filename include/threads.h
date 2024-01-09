#pragma once

#include "context.h"

#include "mems/mems.h"
#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h"

namespace thread {

extern bool flag_exit;

void capture(Spinnaker::CameraPtr &camera,
             Threading::FastIO<cv::Mat> &pipe_out);

void capture(Spinnaker::CameraPtr &camera,
             std::vector<Threading::FastIO<cv::Mat> *> pipes_out,
             Threading::FastIO<mems::Position> &pos_real);

void stack(Threading::FastIO<cv::Mat> &pipe_in,
           Threading::FastIO<cv::Mat> &pipe_out, const size_t n);

void display(Threading::FastIO<cv::Mat> &pipe_tile_a,
             std::vector<Threading::FastIO<cv::Mat> *> pipe_tile_b);

void mems(USB::SerialDevice &device, Threading::FIFO<mems::Position> &pos_in,
          Threading::FastIO<mems::Position> &pos_out);

void aruco(Threading::FastIO<cv::Mat> &pipe_mat_in,
           Threading::FastIO<std::vector<context::ArUcoInfo>> &pipe_info_out,
           bool transform = false);

void track_pid(
    Threading::FastIO<std::vector<context::ArUcoInfo>> &wide_info_in,
    Threading::FastIO<std::vector<context::ArUcoInfo>> &fovea_info_in,
    Threading::FIFO<mems::Position> &mems_pos_next,
    Threading::FastIO<mems::Position> &mems_pos_back);

} // namespace thread
