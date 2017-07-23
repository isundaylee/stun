#pragma once

#include <event/Condition.h>
#include <event/EventLoop.h>

#include <vector>
#include <functional>

namespace event {

class Action {
public:
  Action(std::vector<Condition*> conditions);

  void invoke();
  bool canInvoke();

  template<typename T, void (T::*Method)()>
  void setCallback(T* object) {
    methodCallback_ = [=](void* object) {
      T::Method((T*) object);
    };

    callback_ = [this]() {
      methodCallback_(this->target_);
    };
  }

  void setCallback(std::function<void ()> callback) {
    callback_ = callback;
  }

  void setTarget(void* target) {
    if (target == nullptr) {
      throw std::runtime_error("Cannot set target to nullptr.");
    }

    target_ = target;
  }

private:
  std::vector<Condition*> conditions_;
  std::function<void ()> callback_;
  std::function<void (void*)> methodCallback_;
  void* target_ = nullptr;
};

}
