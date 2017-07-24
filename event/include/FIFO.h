#pragma once

#include <event/Condition.h>

#include <memory>
#include <queue>

namespace event {

template <typename T> class FIFO {
public:
  FIFO(std::size_t capacity)
      : capacity_(capacity), queue_(), canPush_(new Condition()),
        canPop_(new Condition()) {
    updateConditions();
  }

  Condition* canPush() { return canPush_.get(); }

  Condition* canPop() { return canPop_.get(); }

  void push(T element) {
    if (queue_.size() >= capacity_) {
      throw std::runtime_error("Trying to push into a full FIFO.");
    }

    queue_.push(element);
    updateConditions();
  }

  T pop() {
    if (queue_.size() == 0) {
      throw std::runtime_error("Trying to pop from an empty FIFO.");
    }

    T result = queue_.front();
    queue_.pop();
    updateConditions();
    return result;
  }

  T const& front() {
    if (queue_.size() == 0) {
      throw std::runtime_error("Trying to call front() on an empty FIFO.");
    }

    return queue_.front();
  }

private:
  FIFO(FIFO const& copy) = delete;
  FIFO& operator=(FIFO const& copy) = delete;

  FIFO(FIFO&& move) = delete;
  FIFO& operator=(FIFO&& move) = delete;

  std::size_t capacity_;
  std::queue<T> queue_;

  std::unique_ptr<Condition> canPush_;
  std::unique_ptr<Condition> canPop_;

  void updateConditions() {
    canPush_->value = (queue_.size() < capacity_);
    canPop_->value = (queue_.size() > 0);
  }
};
}
