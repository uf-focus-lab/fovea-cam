#include <termios.h>
#include <unistd.h>

namespace serial {

int init(const char *portname, speed_t baudrate);

}
