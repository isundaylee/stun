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

  struct MessageHeader {
    constexpr static size_t WireSize = 4;

    uint8_t version;
    uint8_t unused;
    uint16_t size;

    MessageHeader(uint16_t size_) : version(0x01), unused(0), size(size_) {}
    MessageHeader() : MessageHeader{0} {}

    // serialize()

    void serialize(Byte* buffer) {
      buffer[0] = version;
      buffer[1] = unused;
      *static_cast<uint16_t*>(static_cast<void*>(buffer + 2)) = htons(size);
    }

    static MessageHeader deserialize(Byte* buffer) {
      uint8_t version = buffer[0];
      uint8_t unused = buffer[1];
      uint16_t size =
          ntohs(*static_cast<uint16_t*>(static_cast<void*>(buffer + 2)));

      assertTrue(version == 0x01,
                 "Unexpected TCPCoreDataPipe::MessageHeader::version.");
      assertTrue(unused == 0x00,
                 "Unexpected TCPCoreDataPipe::MessageHeader::unused.");

      return MessageHeader{size};
    }
  };

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
