#pragma once

#include "exception.h"

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace threading {

template <typename T> class FIFO {
private:
  std::queue<T> queue;
  std::mutex mutex;
  std::condition_variable cond_r, cond_w;
  bool closed = false;
  // Maximum size of queue, 0 for unlimited
  size_t max_size = 0;

  void push(T data) {
    std::unique_lock<std::mutex> lock(mutex);
    while (max_size > 0 && queue.size() >= max_size && !closed)
      cond_r.wait(lock);
    if (closed) {
      lock.unlock();
      cond_w.notify_all();
      throw threading::END();
    }
    queue.push(data);
    cond_w.notify_all();
  }

  void push(T &&data) {
    std::unique_lock<std::mutex> lock(mutex);
    while (max_size > 0 && queue.size() >= max_size && !closed)
      cond_r.wait(lock);
    if (closed) {
      lock.unlock();
      cond_w.notify_all();
      throw threading::END();
    }
    queue.push(data);
    cond_w.notify_all();
  }

public:
  FIFO(size_t max_size = 0) : max_size(max_size) {}

  bool empty() {
    std::unique_lock<std::mutex> lock(mutex);
    return queue.empty();
  }

  void write(T *data) { push(*data); }
  void write(T &data) { push(data); }
  void write(T &&data) { push(data); }

  T read() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.empty() && !closed)
      cond_w.wait(lock);
    if (closed) {
      lock.unlock();
      cond_r.notify_all();
      throw threading::END();
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

} // namespace threading
