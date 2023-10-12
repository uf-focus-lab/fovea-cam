#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace FlushingPipe {

class PipeEnd : public std::exception {};

template <class T> class Pipe {
private:
  std::mutex mutex;
  std::condition_variable cond_next;
  T *buffer = nullptr;
  bool open = true;

  void clear_buffer();

public:
  void close();
  void write(const T &value);
  T read();
};

} // namespace FlushingPipe