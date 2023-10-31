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
  std::condition_variable cond_r, cond_w;
  bool closed = false;
  size_t max_size = 0;

public:
  FIFO(size_t max_size = 0) : max_size(max_size) {}
  // Data passed in should be allocated with new
  void write(T &data) {
    std::unique_lock<std::mutex> lock(mutex);
    while ((max_size == 0 || queue.size() >= max_size) && !closed)
      cond_r.wait(lock);
    if (closed) {
      lock.unlock();
      cond_w.notify_all();
      throw Threading::Closed();
    }
    queue.push(std::make_shared<T>(std::move(data)));
    cond_w.notify_all();
  }

  std::shared_ptr<T> read() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.empty() && !closed)
      cond_w.wait(lock);
    if (closed) {
      lock.unlock();
      cond_r.notify_all();
      throw Threading::Closed();
    }
    auto ptr = queue.front();
    queue.pop();
    lock.unlock();
    cond_r.notify_all();
    return ptr;
  }

  void close(bool wait_empty = false) {
    std::unique_lock<std::mutex> lock(mutex);
    if (wait_empty) {
      while (!queue.empty())
        cond_r.wait(lock);
    }
    closed = true;
    lock.unlock();
    cond_r.notify_all();
    cond_w.notify_all();
  }
};

} // namespace Threading
