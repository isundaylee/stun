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
static const int kIOSTunnelPendingPacketQueueSize = 32;

const size_t kTunnelPacketSize = 2048;

struct TunnelPacket : public Packet {
public:
  TunnelPacket() : Packet(kTunnelPacketSize) {}
};

class Tunnel {
public:
  Tunnel();

#if IOS
  using Sender = std::function<void(TunnelPacket packet)>;
  using Receiver = std::function<
      std::shared_ptr<event::Promise<std::vector<TunnelPacket>>>()>;

  Tunnel(Sender sender, Receiver receiver);
  Tunnel(Tunnel&& move);
#endif

#if OSX || LINUX
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

#if IOS
  std::unique_ptr<event::ComputedCondition> canRead_;
  std::unique_ptr<event::ComputedCondition> canWrite_;

  Sender sender_;
  Receiver receiver_;

  std::unique_ptr<event::Timer> receiveTimer_;
  std::unique_ptr<event::Action> receiveAction_;
  std::unique_ptr<event::FIFO<TunnelPacket>> pendingPackets_{
      new event::FIFO<TunnelPacket>(kIOSTunnelPendingPacketQueueSize)};

  void doReceive();

  bool calculateCanRead() const;
  bool calculateCanWrite() const;
#endif
};
};
