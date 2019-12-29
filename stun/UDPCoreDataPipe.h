#pragma once

#include <stun/CoreDataPipe.h>

#include <event/Condition.h>
#include <event/FIFO.h>

#include <networking/Packet.h>
#include <networking/UDPSocket.h>

namespace stun {

class UDPCoreDataPipe : public CoreDataPipe {
public:
  struct ClientConfig {
    networking::SocketAddress addr;
  };

  struct ServerConfig {};

  UDPCoreDataPipe(event::EventLoop& loop, ClientConfig config);
  UDPCoreDataPipe(event::EventLoop& loop, ServerConfig config);

  virtual event::Condition* canSend() override { return socket_->canWrite(); }
  virtual event::Condition* canReceive() override { return socket_->canRead(); }

  virtual bool send(DataPacket packet) override;
  virtual bool receive(DataPacket& output) override;

  int getPort() const { return socket_->getPort().value(); }

private:
  std::unique_ptr<networking::UDPSocket> socket_;
};

}; // namespace stun
