#include "context.h"
#include "mems/mems.h"
#include "threads.h"

#include "cobs/cobs.h"
#include "fcmp/fcmp.h"
#include "threading/exception.h"
#include "threading/fast_io.h"
#include "threading/fifo.h"
#include "util/time.h"

#include <cerrno>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

struct {
  size_t size;
  uint8_t buf[4096];
} serial_rx = {0}, fcmp_tx = {0};

struct {
  unsigned ack, rej;
  std::mutex mutex;
  std::condition_variable updated;
} self;

cobs_buffer_t cobs_rx, cobs_tx;

#define SEND_TO_MEMS(SERIAL, FIELD, VALUE)                                     \
  {                                                                            \
    cobs_reset(&cobs_tx);                                                      \
    fcmp_tx.size = fcmp_compose_frame(fcmp_tx.buf, FIELD, VALUE);              \
    int ret = cobs_encode(&cobs_tx, fcmp_tx.buf, fcmp_tx.size);                \
    if (ret < 0)                                                               \
      throw std::runtime_error("COBS encode error " + std::to_string(ret));    \
    SERIAL.write((uint8_t *)cobs_tx.data, cobs_tx.length + 1);                 \
  }

#define MEMS_MAX_INPUT 65535.0 // Max Digital Input: 16-bit unsigned integer
#define MEMS_MAX_VOLTAGE 200.0 // Max Analog Output: 200 Volts
#define MEMS_MAX_V_DIFF 180.0  // Max Differential Voltage: 180 Volts
#define DIGITAL_VOLTAGE(X)                                                     \
  (uint16_t)(MEMS_MAX_INPUT * ((double)(X) / MEMS_MAX_VOLTAGE))
#define ANALOG_VOLTAGE(X) (MEMS_MAX_VOLTAGE * ((double)(X) / MEMS_MAX_INPUT))

static inline double clip(double min, double max, double val) {
  if (val < min)
    return min;
  else if (val > max)
    return max;
  else
    return val;
}

static inline void compute_channels(const double &pos, uint16_t &ch1,
                                    uint16_t &ch2,
                                    double bias = (MEMS_MAX_V_DIFF / 2.0)) {
  bias = clip(0.0, MEMS_MAX_VOLTAGE / 2.0, bias);
  // Normalized position within [-2bias, +2bias]
  const double voltage_shift = clip(-bias, bias, pos / 2);
  // Assign digitalized voltages for channels
  ch1 = DIGITAL_VOLTAGE(bias + voltage_shift);
  ch2 = DIGITAL_VOLTAGE(bias - voltage_shift);
}

void recv_thread(USB::SerialDevice &device,
                 Threading::FastIO<context::MEMS_Position> &pos_out);

namespace thread {

#undef LOG_NAME
#define LOG_NAME "[thread::mems]"

void mems(USB::SerialDevice &serial,
          Threading::FIFO<context::MEMS_Position> &pos_in,
          Threading::FastIO<context::MEMS_Position> &pos_out) {
  // FCMP Field Buffer
  static fcmp_field_cfg fcmp_config = {0};
  // SET_BIT(fcmp_config, FCMP_CFG_BIT_LOG);
  SET_BIT(fcmp_config, FCMP_CFG_BIT_MEMS_EN);
  SET_BIT(fcmp_config, FCMP_CFG_BIT_LPF);
  SET_BIT(fcmp_config, FCMP_CFG_BIT_STROBE_SYNC);
  // Setup MEMS driver
  SEND_TO_MEMS(serial, FCMP_METHOD_SET | FCMP_FIELD_CFG, fcmp_config);
  // Start recv thread
  std::thread recv([&]() { recv_thread(serial, pos_out); });
  // Infinite loop until closed
  try {
    while (!flag_exit) {
      // Get next target position
      auto pos = pos_in.read();
      // Compute voltages
      compute_channels(pos.x, pos.field.ch[0], pos.field.ch[1]);
      compute_channels(pos.y, pos.field.ch[2], pos.field.ch[3]);
      // Send position until ACK
      bool flag_next = false;
      while (!flag_next && !flag_exit) {
        std::cout << LOG_NAME " Sending position (" << pos.x << ", " << pos.y
                  << ")" << std::endl;
        // Send frame
        SEND_TO_MEMS(serial, FCMP_METHOD_SET | FCMP_FIELD_POS, pos.field);
        // Check for ACK
        std::unique_lock<std::mutex> lock(self.mutex);
        while (self.ack == 0 && self.rej == 0 && !flag_exit) {
          self.updated.wait_for(lock, std::chrono::milliseconds(100));
        }
        flag_next = self.ack && !self.rej;
        self.ack = self.rej = 0;
        lock.unlock();
      }
    }
  } catch (Threading::END &) {
    // Normal termination
  } catch (std::runtime_error &e) {
    std::cerr << "[thread::mems] " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "[thread::mems] Unknown exception" << std::endl;
  }
  // Make sure COBS is flushed
  serial.write((uint8_t *)"\0\0\0\0", 4);
  CLR_BIT(fcmp_config, FCMP_CFG_BIT_LOG);
  CLR_BIT(fcmp_config, FCMP_CFG_BIT_MEMS_EN);
  CLR_BIT(fcmp_config, FCMP_CFG_BIT_LPF);
  // Disable mems driver
  SEND_TO_MEMS(serial, FCMP_METHOD_SET | FCMP_FIELD_CFG, fcmp_config);
  // Close both pipes
  pos_in.close();
  pos_out.close();
  // Wait for recv thread to terminate
  std::cout << "[thread::mems] waiting for recv thread." << std::endl;
  recv.join();
  std::cout << "[thread::mems] terminated." << std::endl;
}

} // namespace thread

#undef LOG_NAME
#define LOG_NAME "[thread::mems::recv]"

void serial_read(USB::SerialDevice &device) {
  serial_rx.size = device.read((uint8_t *)serial_rx.buf, sizeof(serial_rx.buf),
                               serial_rx.size);
}

void serial_flush(USB::SerialDevice &device) {
  while (true) {
    for (unsigned i = 0; i < serial_rx.size; i++) {
      if (serial_rx.buf[i] == 0) {
        move_ahead(serial_rx.buf, &serial_rx.size, i);
        std::cerr << LOG_NAME " Flushed " << i << " bytes" << std::endl;
        return;
      }
    }
    // All bufferred bytes are non-zero
    serial_rx.size = 0;
    serial_read(device);
  }
}
void recv_thread(USB::SerialDevice &device,
                 Threading::FastIO<context::MEMS_Position> &pos_out) {
  try {
    std::shared_ptr<mems::SyncWindow> current_sync =
        std::make_shared<mems::SyncWindow>(0);
    mems::sync.write(current_sync);
    uint16_t next_pos_tag = 0;
    while (!thread::flag_exit) {
      serial_read(device);
      // Decode COBS
      int ret;
      while ((ret = cobs_decode(&cobs_rx, serial_rx.buf, serial_rx.size)) > 0) {
        // Save the remaining bytes
        move_ahead(serial_rx.buf, &serial_rx.size, ret);
        // Save payload size and reset cobs_rx
        const size_t frame_size = cobs_rx.length;
        cobs_reset(&cobs_rx);
        // Process the FCMP frame
        uint8_t checksum = fcmp_checksum(cobs_rx.data, frame_size);
        if (checksum != 0) {
          std::cerr << LOG_NAME " FCMP Checksum Error: " << (unsigned)checksum
                    << std::endl;
          continue;
        }
        // Get the payload size
        if (frame_size < sizeof(fcmp_frame_t)) {
          std::cerr << LOG_NAME " FCMP Frame Size Too Small (" << frame_size
                    << " Bytes)" << std::endl;
          continue;
        };
        // Process received frame
        const size_t payload_size = frame_size - sizeof(fcmp_frame_t);
        const fcmp_frame_t *frame = (const fcmp_frame_t *)cobs_rx.data;
        const uint8_t method = frame->header & FCMP_METHOD,
                      field = frame->header & FCMP_FIELD;
        if (field == FCMP_FIELD_POS) {
          if (method == FCMP_METHOD_ACK) {
            // Handle ACK:POS
            const fcmp_field_pos *pos = (const fcmp_field_pos *)frame->field;
            // Check payload size
            if (payload_size != sizeof(fcmp_field_pos)) {
              std::cerr << LOG_NAME " FCMP Position Payload Size Mismatch (Got "
                        << (unsigned)payload_size << " Bytes, Expecting "
                        << sizeof(fcmp_field_pos) << " Bytes)" << std::endl;
              continue;
            }
            // Update synchronization window
            current_sync = current_sync->conclude(next_pos_tag);
            mems::sync.write(current_sync);
            next_pos_tag = pos->tag;
            // std::cout << LOG_NAME " ACK:POS " << Time::us() << std::endl;
            { // Update acknowledge count
              std::lock_guard<std::mutex> lock(self.mutex);
              self.ack++;
              self.updated.notify_all();
            }
            // Push position to output pipe
            pos_out.write(context::MEMS_Position(
                ANALOG_VOLTAGE(pos->ch[0]) - ANALOG_VOLTAGE(pos->ch[1]),
                ANALOG_VOLTAGE(pos->ch[2]) - ANALOG_VOLTAGE(pos->ch[3])));
          } else if (method == FCMP_METHOD_REJ) {
            { // Update rejection count
              std::lock_guard<std::mutex> lock(self.mutex);
              self.rej++;
              self.updated.notify_all();
            }
            std::string message((const char *)frame->field, payload_size);
            std::cerr << LOG_NAME " FCMP Position Request Rejected (" << message
                      << ")" << std::endl;
            continue;
            // fcmp_log_frame(method, field, frame->field, payload_size);
          }
        } else if (field == FCMP_FIELD_ANY) {
          std::string type = "(?)";
          if (method == FCMP_METHOD_LOG)
            type = "LOG";
          else if (method == FCMP_METHOD_REJ)
            type = "REJ";
          fprintf(stderr, LOG_NAME " FCMP %s:ANY (%lu) %.*s\n", type.c_str(),
                  payload_size, (int)payload_size, frame->field);
        }
      }
      if (ret == 0) {
        // No complete frame, serial buffer all consumed
        serial_rx.size = 0;
      } else { // ret < 0
        std::cerr << LOG_NAME " COBS Error: " << ret << std::endl;
        serial_flush(device);
      }
    }
  } catch (Threading::END &) {
    // Normal termination
  } catch (std::runtime_error &e) {
    std::cerr << LOG_NAME " " << e.what() << std::endl;
  } catch (...) {
    std::cerr << LOG_NAME " Unknown exception" << std::endl;
  }
  std::cout << LOG_NAME " terminated." << std::endl;
}
