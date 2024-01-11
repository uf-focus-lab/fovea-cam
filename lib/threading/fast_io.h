#pragma once
#include "exception.h"

#include <condition_variable> // IWYU pragma: export
#include <memory>
#include <mutex>

namespace Threading {

template <typename T> class FastIO {
private:
  std::mutex mutex;
  std::shared_ptr<const T> ptr = nullptr;
  bool open = true;

  void assign(std::shared_ptr<const T> &&ptr) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!open)
      throw END();
    this->ptr = ptr;
  }

  void assign(std::shared_ptr<const T> &ptr) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!open)
      throw END();
    this->ptr = ptr;
  }

public:
  ~FastIO() { close(); }

  std::shared_ptr<const T> read() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!open)
      throw END();
    return ptr;
  }

  void write(std::shared_ptr<const T> &data) { assign(data); }
  void write(const T &data) { assign(std::make_shared<const T>(data)); }
  void write(const T &&data) {
    assign(std::make_shared<const T>(std::move(data)));
  }
  void write(const T *data) { assign(std::shared_ptr<const T>(data)); }

  void close() {
    std::lock_guard<std::mutex> lock(mutex);
    open = false;
    ptr = nullptr;
  }
};

} // namespace Threading
