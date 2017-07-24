#pragma once

#include <functional>

namespace event {

class Callback {
public:
  Callback() {}

  Callback& operator=(std::function<void ()> func) {
    func_ = func;
    target_ = nullptr;
  }

  Callback(Callback&& move) :
      func_(std::move(move.func_)),
      method_(std::move(move.method_)),
      target_(std::move(move.target_)) {}

  Callback& operator=(Callback&& move) {
    std::swap(func_, move.func_);
    std::swap(method_, move.method_);
    std::swap(target_, move.target_);
  }

  void invoke() {
    if (target_ == nullptr) {
      func_();
    } else {
      method_(target_);
    }
  }

private:
  Callback(Callback const& copy) = delete;
  Callback& operator=(Callback const& copy) = delete;

  std::function<void ()> func_;
  std::function<void (void*)> method_;
  void* target_ = nullptr;
};

}
