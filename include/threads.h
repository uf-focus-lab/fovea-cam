#include "context.h"

#include "threading/fifo.h"
#include "threading/flushing_pipe.h"
#include "usb/serial_device.h"
#include "util/spinnaker.h"

#include <vector>

extern bool flag_exit;

namespace thread {

void capture(Spinnaker::CameraPtr camera,
             Threading::FlushingPipe<cv::Mat> &pipe_out);

void capture(Spinnaker::CameraPtr camera,
             std::vector<Threading::FlushingPipe<cv::Mat> *> pipes_out);

void stack(Threading::FlushingPipe<cv::Mat> &pipe_in,
           Threading::FlushingPipe<cv::Mat> &pipe_out, const size_t n);

void display(Threading::FlushingPipe<cv::Mat> &pipe_tile_a,
             std::vector<Threading::FlushingPipe<cv::Mat> *> pipe_tile_b);

void mems(USB::SerialDevice &device,
          Threading::FIFO<context::mems_position> &pos_in,
          Threading::FlushingPipe<context::mems_position> &pos_out);

} // namespace thread
