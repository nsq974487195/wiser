#ifndef COMPRESSION_H
#define COMPRESSION_H

#include "utils.h"

class VarintBuffer {
 public:
  void Append(uint32_t val) {
    int len = utils::varint_expand_and_encode(val, &data_, end_);
    end_ += len;
  }

  void Prepend(uint32_t val) {
    std::string tmp_buf;
    int len = utils::varint_expand_and_encode(val, &tmp_buf, 0);
    data_.insert(0, tmp_buf, 0, len);
    end_ += len;
  }

  int Size() {
    return end_;
  }

  int End() {
    return end_;
  }

  std::string Data() {
    return std::string(data_, 0, end_);
  }

 private:
  std::string data_;
  int end_ = 0;
};

#endif