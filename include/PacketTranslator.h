#pragma once

#include <FIFO.h>
#include <Util.h>

#include <ev/ev++.h>

#include <functional>

namespace stun {

const ev_tstamp kPacketTranslatorInterval = 0.001;

template <typename X, typename Y>
class PacketTranslator {
public:
  PacketTranslator(FIFO<X>& source, FIFO<Y>& target) :
      source_(source),
      target_(target) {
    timer_.set<PacketTranslator, &PacketTranslator::doTranslate>(this);
    timer_.set(0.0, kPacketTranslatorInterval);
    timer_.start();
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

  void doTranslate(ev::timer& watcher, int events) {
    if (events & EV_ERROR) {
      throwUnixError("PacketTranslator doTranslate()");
    }

    while (!source_.empty() && !target_.full()) {
      X packet = source_.pop();
      target_.push(transform(packet));
    }
  }

  FIFO<X>& source_;
  FIFO<Y>& target_;

  ev::timer timer_;
};

}
