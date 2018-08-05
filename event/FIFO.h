#pragma once

#include <event/Condition.h>

#include <memory>
#include <queue>

namespace event {

template <typename T> class FIFO {
public:
  FIFO(std::size_t capacity)
      : capacity_(capacity), queue_(), canPush_(new BaseCondition()),
        canPop_(new BaseCondition()) {
    updateConditions();
  }

  Condition* canPush() const { return canPush_.get(); }

  Condition* canPop() const { return canPop_.get(); }

  void push(T&& element) {
    if (queue_.size() >= capacity_) {
      throw std::runtime_error("Trying to push into a full FIFO.");
    }

    queue_.push(std::move(element));
    updateConditions();
  }

  T pop() {
    if (queue_.size() == 0) {
      throw std::runtime_error("Trying to pop from an empty FIFO.");
    }

    T result(std::move(queue_.front()));
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

  std::unique_ptr<BaseCondition> canPush_;
  std::unique_ptr<BaseCondition> canPop_;

  void updateConditions() {
    canPush_->set(queue_.size() < capacity_);
    canPop_->set(queue_.size() > 0);
  }
};
} // namespace event
