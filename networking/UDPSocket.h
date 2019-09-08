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
private:
  common::Logger logger_;

public:
  UDPSocket(event::EventLoop& loop, std::string logPrefix,
            NetworkType networkType)
      : Socket(loop, logPrefix, networkType, UDP),
        logger_{common::Logger::getDefault("Socket")} {
    logger_.setPrefix(logPrefix);
  }

  UDPSocket(event::EventLoop& loop, NetworkType networkType)
      : UDPSocket(loop, "", networkType) {}

  void write(UDPPacket packet);
  bool read(UDPPacket& packet);
};
} // namespace networking
