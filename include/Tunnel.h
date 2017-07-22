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

struct TunnelPacket {
  char* buffer;
  int size;

  TunnelPacket() {
    size = 0;
    buffer = new char[kTunnelBufferSize];
  }

  TunnelPacket(TunnelPacket const& copy) {
    size = copy.size;
    buffer = new char[kTunnelBufferSize];
    std::copy(copy.buffer, copy.buffer + copy.size, buffer);
  }

  TunnelPacket& operator=(TunnelPacket copy) {
    swap(copy, *this);
    return *this;
  }

  TunnelPacket(TunnelPacket&& other) {
    using std::swap;
    swap(buffer, other.buffer);
    swap(size, other.size);
  }

  ~TunnelPacket() {
    free(buffer);
  }

  friend void swap(TunnelPacket& lhs, TunnelPacket& rhs) noexcept {
    using std::swap;
    swap(lhs.buffer, rhs.buffer);
    swap(lhs.size, rhs.size);
  }
};

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
