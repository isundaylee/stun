#pragma once

#include <Util.h>

#include <event/Action.h>
#include <event/FIFO.h>

#include <functional>

namespace stun {

template <typename X, typename Y>
class PacketTranslator {
public:
  PacketTranslator(event::FIFO<X>* source, event::FIFO<Y>* target) :
      source_(source),
      target_(target),
      translator_() {}

  void start() {
    translator_.reset(new event::Action({source_->canPop(), target_->canPush()}));
    translator_->callback.setMethod<PacketTranslator, &PacketTranslator::doTranslate>(this);
  }

  std::function<Y (X const&)> transform = [](X const& t) {
    throw std::runtime_error("transform not set in PacketTranslator");
    return Y();
  };

private:
  PacketTranslator(PacketTranslator const& copy) = delete;
  PacketTranslator& operator+=(PacketTranslator const& copy) = delete;
  PacketTranslator(PacketTranslator&& move) = delete;
  PacketTranslator& operator+=(PacketTranslator&& move) = delete;

  void doTranslate() {
    while (source_->canPop()->value && target_->canPush()->value) {
      X packet = source_->pop();
      Y result = transform(packet);
      if (result.size > 0) {
        target_->push(result);
      }
    }
  }

  event::FIFO<X>* source_;
  event::FIFO<Y>* target_;
  std::unique_ptr<event::Action> translator_;
};

}
