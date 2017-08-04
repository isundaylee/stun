#pragma once

#include <networking/Packet.h>

#include <common/FileDescriptor.h>
#include <event/Condition.h>

#include <stdio.h>

#include <string>

namespace networking {

class TunnelClosedException : public std::runtime_error {
public:
  TunnelClosedException(std::string const& reason)
      : std::runtime_error(reason) {}
};

static const unsigned int kTunnelEthernetMTU = 1444;
static const int kTunnelBufferSize = 2000;

const size_t kTunnelPacketSize = 2048;

struct TunnelPacket : public Packet {
public:
  TunnelPacket() : Packet(kTunnelPacketSize) {}
};

class Tunnel {
public:
  Tunnel();
  Tunnel(Tunnel&& move) = default;

  std::string deviceName;

  bool read(TunnelPacket& packet);
  bool write(TunnelPacket packet);

  event::Condition* canRead() const;
  event::Condition* canWrite() const;

private:
  Tunnel(const Tunnel&) = delete;
  Tunnel& operator=(const Tunnel&) = delete;

  common::FileDescriptor fd_;
};
};
