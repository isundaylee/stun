#pragma once

#include <stun/CoreDataPipe.h>

#include <common/Util.h>

#include <event/Action.h>
#include <event/Condition.h>

#include <networking/Packet.h>
#include <networking/TCPServer.h>
#include <networking/TCPSocket.h>

namespace stun {

class TCPCoreDataPipe : public CoreDataPipe {
public:
  enum class Role {
    CLIENT,
    SERVER,
  };

  struct ClientConfig {
    networking::SocketAddress addr;
  };

  struct ServerConfig {};

  struct __attribute__((packed)) MessageHeader {
    uint8_t version;
    uint8_t unused;
    uint16_t size;

    MessageHeader(uint16_t size_) : version(0x01), unused(0), size(size_) {}
    MessageHeader() : MessageHeader{0} {}
  };

  static_assert(sizeof(MessageHeader) == 4);

  TCPCoreDataPipe(event::EventLoop& loop, ClientConfig config);
  TCPCoreDataPipe(event::EventLoop& loop, ServerConfig config);

  virtual event::Condition* canSend() override { return canSend_.get(); }
  virtual event::Condition* canReceive() override { return canReceive_.get(); }

  virtual bool send(DataPacket packet) override;
  virtual bool receive(DataPacket& output) override;

  int getPort() const {
    return (role_ == Role::SERVER ? server_->getPort().value()
                                  : socket_->getPort().value());
  }

private:
  Role role_;

  std::unique_ptr<networking::TCPServer> server_;
  std::unique_ptr<networking::TCPSocket> socket_;

  std::unique_ptr<event::ComputedCondition> canSend_;
  std::unique_ptr<event::ComputedCondition> canReceive_;

  // Sending state
  networking::Packet packetToSend_{sizeof(MessageHeader) + DataPacket::Size};
  size_t packetBytesSent_ = 0;

  // Receiving state
  constexpr static size_t ReceiveBufferSize =
      2 * (sizeof(MessageHeader) + DataPacket::Size);
  networking::Packet receiveBuffer_{ReceiveBufferSize};
};

}; // namespace stun
