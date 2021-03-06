#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <iostream>
#include <cassert>

#include <glog/logging.h>

class BufferPool {
 public:
  BufferPool(){}

  BufferPool(const int n_buffers, const int buffer_size)
  {
    Reset(n_buffers, buffer_size);
  }

  void Reset(const int n_buffers, const int buffer_size) {
    buffer_size_ = buffer_size;
    pool_.clear();

    for (int i = 0; i < n_buffers; i++) {
      std::unique_ptr<char[]> p(new char[buffer_size]);
      pool_.push_back(std::move(p));
    }
  }

  ~BufferPool() {} 

  BufferPool(BufferPool &&pool) = delete;
  BufferPool(const BufferPool &pool) = delete;
  BufferPool &operator = (const BufferPool &pool) = delete;

  std::unique_ptr<char[]> Get() {
    std::lock_guard<std::mutex> lock(mutex_);

    // Dynamically grow
    if (pool_.size() == 0) {
      std::unique_ptr<char[]> p(new char[buffer_size_]);
      pool_.push_back(std::move(p));
    }

    std::unique_ptr<char[]> p = std::move(pool_.back());
    pool_.pop_back();

    return p;
  }

  void Put(std::unique_ptr<char[]> p) {
    std::lock_guard<std::mutex> lock(mutex_);
    pool_.push_back(std::move(p));
  }

  int Size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return pool_.size();
  }

 private:
  std::vector<std::unique_ptr<char[]>> pool_;
  std::mutex mutex_;
  int buffer_size_;
};



