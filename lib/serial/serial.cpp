#include "serial.h"
#include <fcntl.h>
#include <stdio.h>

namespace serial {

int init(const char *path, speed_t baudrate) {
  // Open serial port by path
  const int fd = open(path, O_RDWR | O_NOCTTY | O_NDELAY);
  if (fd == -1) {
    perror("[serial::open()] Unable to open port");
    return -1;
  }
  // Get attributes of the serial port
  struct termios attr;
  if (tcgetattr(fd, &attr) < 0) {
    perror("[serial::open()] get attribute error");
    close(fd);
    return -1;
  }
  // Set baudrate
  cfsetispeed(&attr, baudrate);
  cfsetospeed(&attr, baudrate);
  // Make raw
  cfmakeraw(&attr);
  /*
    // 8N1
    attr.c_cflag &= ~PARENB;
    attr.c_cflag &= ~CSTOPB;
    attr.c_cflag &= ~CSIZE;
    attr.c_cflag |= CS8;
    // no flow control
    attr.c_cflag &= ~CRTSCTS;
    attr.c_cflag |= CREAD | CLOCAL;          // turn on READ & ignore ctrl
    lines attr.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    attr.c_oflag &= ~OPOST;
    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 1;
  */
  // Configure the device : 8 bits, no parity, no control
  attr.c_cflag |= (CLOCAL | CREAD | CS8);
  attr.c_iflag |= (IGNPAR | IGNBRK);
  attr.c_oflag &= ~OPOST;
  // Set attributes
  if (tcsetattr(fd, TCSANOW, &attr) < 0) {
    perror("[serial::open()] set attribute error");
    close(fd);
    return -1;
  }
  // Return serial port's file descriptor
  return fd;
}

} // namespace serial
