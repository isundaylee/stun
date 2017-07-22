#pragma once

#include <deque>
#include <stdexcept>
#include <iostream>

namespace stun {

template <typename T>
class FIFO {
public:
  FIFO(int capacity) {
    capacity_ = capacity;
  }

  void push(T const& element) {
      if (data_.size() == capacity_) {
        throw std::runtime_error("Attempting to push into a full FIFO.");
      }
      data_.push_back(element);
  }

  T pop() {
    if (data_.size() == 0) {
      throw std::runtime_error("Attempting to pop from an empty FIFO.");
    }
    T front = data_.front();
    data_.pop_front();
    return front;
  }

  bool empty() const {
    return data_.size() == 0;
  }

  bool full() const {
    return data_.size() == capacity_;
  }

private:
  std::deque<T> data_;
  int capacity_;
};

}
