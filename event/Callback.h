#pragma once

#include <functional>
#include <stdexcept>

namespace event {

template <typename R> class Callback {
public:
  Callback() {}

  Callback(Callback&& move)
      : target(std::move(move.target)), func_(std::move(move.func_)),
        method_(std::move(move.method_)) {}

  Callback& operator=(Callback&& move) {
    std::swap(func_, move.func_);
    std::swap(method_, move.method_);
    std::swap(target, move.target);
    return *this;
  }

  Callback& operator=(std::function<R()> func) {
    func_ = func;
    method_ = nullptr;
    target = nullptr;
    return *this;
  }

  template <typename T, R (T::*Method)()> void setMethod(T* object) {
    func_ = nullptr;
    method_ = [](void* object) { return (((T*)object)->*Method)(); };
    target = object;
  }

  R invoke() {
    if (!!func_) {
      return func_();
    } else if (!!method_) {
      if (target == nullptr) {
        throw std::runtime_error("Invoking an Callable with an empty target.");
      }
      return method_(target);
    } else {
      throw std::runtime_error("Invoking an empty Callable.");
    }
  }

  void* target = nullptr;

private:
  Callback(Callback const& copy) = delete;
  Callback& operator=(Callback const& copy) = delete;

  std::function<R()> func_;
  std::function<R(void*)> method_;
};
}
