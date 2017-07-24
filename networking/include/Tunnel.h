#pragma once

#include <networking/Pipe.h>

#include <stdio.h>

#include <string>

namespace networking {

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
