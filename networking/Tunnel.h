#pragma once

#include <networking/Packet.h>

#include <common/FileDescriptor.h>
#include <common/Util.h>
#include <event/Action.h>
#include <event/Condition.h>
#include <event/FIFO.h>
#include <event/Promise.h>
#include <event/Timer.h>

#include <stdio.h>

#include <string>

namespace networking {

using TunnelClosedException = common::FileDescriptor::ClosedException;

static const unsigned int kTunnelEthernetMTU = 1444;
static const int kTunnelBufferSize = 2000;
static const int kIOSTunnelPendingPacketQueueSize = 128;

const size_t kTunnelPacketSize = 2048;

struct TunnelPacket : public Packet {
public:
  TunnelPacket() : Packet(kTunnelPacketSize) {}
};

class Tunnel {
public:
  Tunnel();

#if TARGET_IOS
  using Sender = std::function<void(TunnelPacket packet)>;
  using PacketsPromise = event::Promise<std::vector<TunnelPacket>>;
  using Receiver = std::function<std::shared_ptr<PacketsPromise>()>;

  Tunnel(Sender sender, Receiver receiver);
  Tunnel(Tunnel&& move);
#endif

#if TARGET_OSX || TARGET_LINUX
  Tunnel(Tunnel&& move) = default;
#endif

  std::string deviceName;

  bool read(TunnelPacket& packet);
  bool write(TunnelPacket packet);

  event::Condition* canRead() const;
  event::Condition* canWrite() const;

private:
  Tunnel(const Tunnel&) = delete;
  Tunnel& operator=(const Tunnel&) = delete;

  common::FileDescriptor fd_;

#if TARGET_IOS
  std::unique_ptr<event::ComputedCondition> canRead_;
  std::unique_ptr<event::ComputedCondition> canWrite_;
  std::unique_ptr<event::ComputedCondition> canReceive_;

  Sender sender_;
  Receiver receiver_;

  std::unique_ptr<event::Action> receiveAction_;
  std::unique_ptr<event::FIFO<TunnelPacket>> pendingPackets_{
      new event::FIFO<TunnelPacket>(kIOSTunnelPendingPacketQueueSize)};
  std::shared_ptr<PacketsPromise> packetsPromise_;

  void doReceive();

  bool calculateCanRead();
  bool calculateCanWrite();
  bool calculateCanReceive();
#endif
};
};
