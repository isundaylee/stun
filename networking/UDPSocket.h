#pragma once

#include <networking/Packet.h>
#include <networking/Socket.h>

namespace networking {

static const size_t kUDPPacketSize = 2048;

class UDPPacket : public Packet {
public:
  UDPPacket() : Packet(kUDPPacketSize) {}
};

class UDPSocket : public Socket {
public:
  UDPSocket(NetworkType networkType) : Socket(networkType, UDP) {}

  void write(UDPPacket packet);
  bool read(UDPPacket& packet);
};
} // namespace networking
