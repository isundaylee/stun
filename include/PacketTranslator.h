#pragma once

#include <FIFO.h>
#include <Util.h>

#include <ev/ev++.h>

#include <functional>

namespace stun {

const ev_tstamp kPacketTranslatorInterval = 0.01;

template <typename P>
class PacketTranslator {
public:
  PacketTranslator(FIFO<P>& source, FIFO<P>& target) :
      source_(source),
      target_(target) {

    timer_.set<PacketTranslator, &PacketTranslator::doTranslate>(this);
    timer_.set(0.0, kPacketTranslatorInterval);
    timer_.start();
  }

  std::function<P (P const&)> transform = [](P const& t) {
    return t;
  };

private:
  PacketTranslator(PacketTranslator const& copy) = delete;
  PacketTranslator& operator+=(PacketTranslator const& copy) = delete;

  void doTranslate(ev::timer& watcher, int events) {
    if (events & EV_ERROR) {
      throwUnixError("PacketTranslator doTranslate()");
    }

    while (!source_.empty()) {
      P packet = source_.pop();
      target_.push(transform(packet));
      LOG() << "Translated 1 packet with size " << packet.size << std::endl;
    }
  }

  FIFO<P>& source_;
  FIFO<P>& target_;

  ev::timer timer_;
};

}
