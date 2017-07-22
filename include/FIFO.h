#pragma once

#include <deque>
#include <stdexcept>
#include <iostream>
#include <functional>

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

      if (data_.size() == 1) {
        onBecomeNonEmpty();
      }
      if (data_.size() == capacity_) {
        onBecomeFull();
      }
  }

  T pop() {
    if (data_.size() == 0) {
      throw std::runtime_error("Attempting to pop from an empty FIFO.");
    }
    T front = data_.front();
    data_.pop_front();

    if (data_.size() == capacity_ - 1) {
      onBecomeNonFull();
    }
    if (data_.size() == 0) {
      onBecomeEmpty();
    }

    return front;
  }

  T const& front() {
    if (data_.size() == 0) {
      throw std::runtime_error("Attempting to access front of an empty FIFO.");
    }
    return data_.front();
  }

  bool empty() const {
    return data_.size() == 0;
  }

  bool full() const {
    return data_.size() == capacity_;
  }

  std::function<void ()> onBecomeEmpty = []() {};
  std::function<void ()> onBecomeFull = []() {};
  std::function<void ()> onBecomeNonEmpty = []() {};
  std::function<void ()> onBecomeNonFull = []() {};

private:
  std::deque<T> data_;
  int capacity_;
};

}
