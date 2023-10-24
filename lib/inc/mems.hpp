#include <string>
#include <CppLinuxSerial/SerialPort.hpp>

namespace MEMS {

using namespace mn::CppLinuxSerial;

class Proxy {
  private:
    SerialPort serial_port;
    std::string port_name;
    int baud_rate;
    int timeout;
  public:
    Proxy(std::string);
    ~Proxy();
    void send(std::string) {
      serial_port.Write(data);
    }
};

}
