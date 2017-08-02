#pragma once

#include <networking/Packet.h>
#include <networking/Pipe.h>

#include <stdio.h>

#include <string>

namespace networking {

static const unsigned int kTunnelEthernetMTU = 1444;
static const int kTunnelBufferSize = 2000;

enum TunnelType {
  TUN,
  TAP,
};

const size_t kTunnelPacketSize = 2048;

struct TunnelPacket : public Packet {
public:
  TunnelPacket() : Packet(kTunnelPacketSize) {}
};

class Tunnel : public Pipe<TunnelPacket> {
public:
  Tunnel(TunnelType type);
  Tunnel(Tunnel&& move)
      : Pipe(std::move(move)), type_(move.type_), devName_(move.devName_) {}

  std::string const& getDeviceName() { return devName_; }

  void open() override;

protected:
  bool read(TunnelPacket& packet) override;
  bool write(TunnelPacket const& packet) override;

private:
  Tunnel(const Tunnel&) = delete;
  Tunnel& operator=(const Tunnel&) = delete;

  TunnelType type_;
  std::string devName_;
};
};
