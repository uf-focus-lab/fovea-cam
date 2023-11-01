#pragma once

#include "exception.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace Threading {

template <typename T> class FIFO {
private:
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cond_r, cond_w;
  bool closed = false;
  // Maximum size of queue, 0 for unlimited
  size_t max_size = 0;

  void push(T &&data) {
    std::unique_lock<std::mutex> lock(mutex);
    while (max_size > 0 && queue.size() >= max_size && !closed)
      cond_r.wait(lock);
    if (closed) {
      lock.unlock();
      cond_w.notify_all();
      throw Threading::END();
    }
    queue.push(data);
    cond_w.notify_all();
  }

public:
  FIFO(size_t max_size = 0) : max_size(max_size) {}

  void write(T *data) { push(*data); }
  void write(T &data) { push(data); }
  void write(T &&data) { push(std::move(data)); }

  T read() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.empty() && !closed)
      cond_w.wait(lock);
    if (closed) {
      lock.unlock();
      cond_r.notify_all();
      throw Threading::END();
    }
    T data = queue.front();
    queue.pop();
    lock.unlock();
    cond_r.notify_all();
    return data;
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
