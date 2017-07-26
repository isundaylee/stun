#pragma once

#include <functional>
#include <stdexcept>

namespace event {

class Callback {
public:
  Callback() {}

  Callback(Callback&& move)
      : func_(std::move(move.func_)), method_(std::move(move.method_)),
        target(std::move(move.target)) {}

  Callback& operator=(Callback&& move) {
    std::swap(func_, move.func_);
    std::swap(method_, move.method_);
    std::swap(target, move.target);
    return *this;
  }

  Callback& operator=(std::function<void()> func) {
    func_ = func;
    method_ = nullptr;
    target = nullptr;
    return *this;
  }

  template <typename T, void (T::*Method)()> void setMethod(T* object) {
    func_ = nullptr;

    method_ = [](void* object) { (((T*)object)->*Method)(); };

    target = object;
  }

  bool invoke() {
    if (!!func_) {
      func_();
      return true;
    } else if (!!method_) {
      if (target == nullptr) {
        throw std::runtime_error("Invoking an Callable with an empty target.");
      }

      method_(target);
      return true;
    } else {
      return false;
    }
  }

  void* target = nullptr;

private:
  Callback(Callback const& copy) = delete;
  Callback& operator=(Callback const& copy) = delete;

  std::function<void()> func_;
  std::function<void(void*)> method_;
};
}
