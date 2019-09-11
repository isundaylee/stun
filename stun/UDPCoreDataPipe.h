#pragma once

#include <event/Condition.h>
#include <event/FIFO.h>

#include <networking/Packet.h>
#include <networking/UDPSocket.h>

namespace stun {

static const size_t kDataPacketSize = 2048;

class DataPacket : public networking::Packet {
public:
  DataPacket() : Packet(kDataPacketSize) {}
};

class UDPCoreDataPipe {
public:
  struct ClientConfig {
    networking::SocketAddress addr;
  };

  struct ServerConfig {};

  UDPCoreDataPipe(event::EventLoop& loop, ClientConfig config);
  UDPCoreDataPipe(event::EventLoop& loop, ServerConfig config);

  event::Condition* canSend() { return socket_->canWrite(); }
  event::Condition* canReceive() { return socket_->canRead(); }

  bool send(DataPacket packet);
  bool receive(DataPacket& output);

  int getPort() const { return socket_->getPort().value(); }

private:
  std::unique_ptr<networking::UDPSocket> socket_;
};

}; // namespace stun