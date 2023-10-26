#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace FlushingPipe {

class PipeEnd : public std::exception {};

template <typename T> class Pipe {
private:
  std::mutex mutex;
  std::condition_variable cond_next;
  T *buffer = nullptr;
  bool open = true;

  void clear_buffer();

public:
  ~Pipe() { close(); }
  void close();
  void write(const T &value);
  T read();
};

template <typename T> void Pipe<T>::clear_buffer() {
  if (buffer != nullptr) {
    delete buffer;
    buffer = nullptr;
  }
}

template <typename T> void Pipe<T>::write(const T &value) {
  std::unique_lock<std::mutex> lock(mutex);
  if (!open)
    throw PipeEnd();
  clear_buffer();
  buffer = new T(value);
  cond_next.notify_one();
  lock.unlock();
}

template <typename T> void Pipe<T>::close() {
  std::unique_lock<std::mutex> lock(mutex);
  clear_buffer();
  open = false;
  cond_next.notify_one();
  lock.unlock();
}

template <typename T> T Pipe<T>::read() {
  std::unique_lock<std::mutex> lock(mutex);
  do {
    if (!open)
      throw PipeEnd();
    if (buffer != nullptr)
      break;
    cond_next.wait(lock);
  } while (1);
  T value = *buffer;
  lock.unlock();
  return value;
}

} // namespace FlushingPipe