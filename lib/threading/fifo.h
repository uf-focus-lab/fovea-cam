#pragma once
#include "exception.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

namespace Threading {

template <typename T> class FIFO {
private:
  std::queue<std::shared_ptr<T>> queue;
  std::mutex mutex;
  std::condition_variable cond;
  bool closed = false;

public:
  FIFO<T>(size_t size = 256) { queue.reserve(size); }
  // Data passed in should be allocated with new
  void write(T &data) {
    std::unique_lock<std::mutex> lock(mutex);
    if (closed)
      throw Threading::Closed();
    queue.push(std::make_unique<T>(data));
    cond.notify_one();
  }
  std::shared_ptr<T> read() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.empty() && !closed)
      cond.wait(lock);
    if (closed)
      throw Threading::Closed();
    auto ptr = queue.front();
    queue.pop();
    lock.unlock();
    return ptr;
  }
  void close() {
    std::unique_lock<std::mutex> lock(mutex);
    closed = true;
    cond.notify_all();
    lock.unlock();
  }
};

} // namespace Threading
