#include <common/Util.h>

#if IOS

#include "event/Trigger.h"
#include "networking/Tunnel.h"

using namespace std::chrono_literals;

namespace networking {

static const event::Duration kIOSTunnelReceiveInterval = 1000ms;

Tunnel::Tunnel() {
  notImplemented("Default construction of Tunnel is not supported on iOS.");
}

Tunnel::Tunnel(Sender sender, Receiver receiver)
    : sender_(sender), receiver_(receiver) {
  receiveTimer_.reset(new event::Timer(0ms));
  receiveAction_.reset(new event::Action({receiveTimer_->didFire()}));
  receiveAction_->callback.setMethod<Tunnel, &Tunnel::doReceive>(this);
}

Tunnel::Tunnel(Tunnel&& move)
    : deviceName(std::move(move.deviceName)), fd_(std::move(move.fd_)),
      canRead_(std::move(move.canRead_)), canWrite_(std::move(move.canWrite_)),
      sender_(std::move(move.sender_)), receiver_(std::move(move.receiver_)),
      receiveTimer_(std::move(move.receiveTimer_)),
      receiveAction_(std::move(move.receiveAction_)),
      pendingPackets_(std::move(move.pendingPackets_)) {
  receiveAction_->callback.target = this;
}

event::Condition* Tunnel::canRead() const { return canRead_.get(); }

event::Condition* Tunnel::canWrite() const { return canWrite_.get(); }

bool Tunnel::read(networking::TunnelPacket& packet) {
  assertTrue(canRead()->eval(),
             "Attempting to read when canRead() evals to false.");

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
  auto packetsPromise = receiver_();

  event::Trigger::arm({packetsPromise->isReady()}, [this, packetsPromise]() {
    auto packets = packetsPromise->consume();

    while (pendingPackets_->canPush()->eval() && !packets.empty()) {
      pendingPackets_->push(std::move(packets.back()));
      packets.pop_back();

      if (!packets.empty()) {
        LOG_I("Tunnel") << "Dropped " << packets.size()
                        << "packets due to pending packets queue overflow."
                        << std::endl;
      }
    }
  });

  receiveTimer_->extend(kIOSTunnelReceiveInterval);
}

bool Tunnel::calculateCanRead() const {
  return pendingPackets_->canPop()->eval();
}

bool Tunnel::calculateCanWrite() const { return true; }
}

#endif
