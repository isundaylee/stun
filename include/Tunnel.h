#pragma once

#include <FIFO.h>
#include <Pipe.h>

#include <ev/ev++.h>
#include <stdio.h>

#include <string>

namespace stun {

const int kTunnelBufferSize = 2000;

enum TunnelType {
  TUN,
  TAP,
};

struct TunnelPacket: PipePacket {};

class Tunnel: public Pipe<TunnelPacket> {
public:
  Tunnel(TunnelType type);

  std::string const& getDeviceName() {
    return devName_;
  }

private:
  Tunnel(const Tunnel&) = delete;
  Tunnel& operator=(const Tunnel&) = delete;

  bool read(TunnelPacket& packet) override;
  bool write(TunnelPacket const& packet) override;

  TunnelType type_;
  std::string devName_;
};

};
