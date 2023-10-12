#include "flushing_pipe.hpp"

namespace FlushingPipe {

template <class T> void Pipe<T>::clear_buffer() {
  if (buffer != nullptr) {
    delete buffer;
    buffer = nullptr;
  }
}

template <class T> void Pipe<T>::write(const T &value) {
  std::unique_lock<std::mutex> lock(mutex);
  if (!open)
    throw PipeEnd();
  clear_buffer();
  buffer = new T(value);
  cond_next.notify_one();
  lock.unlock();
}

template <class T> void Pipe<T>::close() {
  std::unique_lock<std::mutex> lock(mutex);
  clear_buffer();
  open = false;
  cond_next.notify_one();
  lock.unlock();
}

template <class T> T Pipe<T>::read() {
  std::unique_lock<std::mutex> lock(mutex);
  do {
    if (!open)
      throw PipeEnd();
    if (buffer != nullptr)
      break;
    cond_next.wait(lock);
  } while (1);
  T value = *buffer;
  clear_buffer();
  lock.unlock();
  return value;
}

}; // namespace FlushingPipe
