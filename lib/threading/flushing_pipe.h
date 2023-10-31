#pragma once
#include "exception.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace Threading {

template <typename T> class FlushingPipe {
private:
  std::mutex mutex;
  std::shared_ptr<const T> ptr = nullptr;
  bool open = true;

public:
  ~FlushingPipe() { close(); }

  std::shared_ptr<const T> read() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!open)
      throw Closed();
    auto ptr = this->ptr;
    lock.unlock();
    return ptr;
  }

  void write(const T &data) {
    std::unique_lock<std::mutex> lock(mutex);
    if (!open)
      throw Closed();
    this->ptr = std::make_shared<const T>(std::move(data));
    lock.unlock();
  }

  void close() {
    std::unique_lock<std::mutex> lock(mutex);
    open = false;
    lock.unlock();
  }
};

} // namespace Threading