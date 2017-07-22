#pragma once

#include <FIFO.h>

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

class Tunnel {
public:
  Tunnel(TunnelType type);

  std::string const& getDeviceName() {
    return devName_;
  }

  FIFO<TunnelPacket> inboundQ;
  FIFO<TunnelPacket> outboundQ;

private:
  Tunnel(const Tunnel&) = delete;
  Tunnel& operator=(const Tunnel&) = delete;

  void doReceive(ev::io& watcher, int events);
  void doSend(ev::io& watcher, int events);

  ev::io inboundWatcher_;
  ev::io outboundWatcher_;
  TunnelType type_;
  std::string devName_;
  int fd_;
  char buffer_[kTunnelBufferSize];
};

};
