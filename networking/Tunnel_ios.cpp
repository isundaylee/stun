#include <common/Util.h>

#if TARGET_IOS

#include "event/Trigger.h"
#include "networking/Tunnel.h"

using namespace std::chrono_literals;

namespace networking {

static const event::Duration kIOSTunnelReceiveInterval = 1000ms;

Tunnel::Tunnel(event::EventLoop& loop) : loop_(loop) {
  notImplemented("Default construction of Tunnel is not supported on iOS.");
}

Tunnel::Tunnel(event::EventLoop& loop, Sender sender, Receiver receiver)
    : loop_(loop), sender_(sender), receiver_(receiver),
      pendingPackets_(new event::FIFO<TunnelPacket>(
          loop, kIOSTunnelPendingPacketQueueSize)) {
  canRead_ = loop_.createComputedCondition();
  canRead_->expression.setMethod<Tunnel, &Tunnel::calculateCanRead>(this);

  canWrite_ = loop_.createComputedCondition();
  canWrite_->expression.setMethod<Tunnel, &Tunnel::calculateCanWrite>(this);

  canReceive_ = loop_.createComputedCondition();
  canReceive_->expression.setMethod<Tunnel, &Tunnel::calculateCanReceive>(this);

  receiveAction_ = loop_.createAction("networking::Tunnel::receiveAction_",
                                      {canReceive_.get()});
  receiveAction_->callback.setMethod<Tunnel, &Tunnel::doReceive>(this);
}

Tunnel::Tunnel(Tunnel&& move)
    : deviceName(std::move(move.deviceName)), loop_(move.loop_),
      fd_(std::move(move.fd_)), canRead_(std::move(move.canRead_)),
      canWrite_(std::move(move.canWrite_)),
      canReceive_(std::move(move.canReceive_)),
      sender_(std::move(move.sender_)), receiver_(std::move(move.receiver_)),
      receiveAction_(std::move(move.receiveAction_)),
      pendingPackets_(std::move(move.pendingPackets_)),
      packetsPromise_(std::move(move.packetsPromise_)) {
  receiveAction_->callback.target = this;
}

event::Condition* Tunnel::canRead() const { return canRead_.get(); }

event::Condition* Tunnel::canWrite() const { return canWrite_.get(); }

bool Tunnel::read(networking::TunnelPacket& packet) {
  if (!pendingPackets_->canPop()->eval()) {
    return false;
  }

  packet = pendingPackets_->pop();
  return true;
}

bool Tunnel::write(networking::TunnelPacket packet) {
  assertTrue(canWrite()->eval(),
             "Attempting to write when canWrite() evals to false.");

  sender_(std::move(packet));
  return true;
}

void Tunnel::doReceive() {
  if (!!packetsPromise_) {
    // Processing packets from last call
    auto packets = packetsPromise_->consume();

    while (pendingPackets_->canPush()->eval() && !packets.empty()) {
      pendingPackets_->push(std::move(packets.back()));
      packets.pop_back();
    }

    if (!packets.empty()) {
      LOG_I("Tunnel") << "Dropped " << packets.size()
                      << " packets due to pending packets queue overflow."
                      << std::endl;
    }
  }

  packetsPromise_ = receiver_();
}

bool Tunnel::calculateCanRead() { return pendingPackets_->canPop()->eval(); }

bool Tunnel::calculateCanWrite() { return true; }

bool Tunnel::calculateCanReceive() {
  return (!packetsPromise_) || packetsPromise_->isReady()->eval();
}
} // namespace networking

#endif
