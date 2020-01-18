#include "stun/TCPCoreDataPipe.h"

namespace stun {

TCPCoreDataPipe::TCPCoreDataPipe(event::EventLoop& loop, ClientConfig config)
    : CoreDataPipe{}, role_{Role::CLIENT}, socket_{new networking::TCPSocket{
                                               loop, config.addr.type}},
      canSend_{loop.createComputedCondition()},
      canReceive_{loop.createComputedCondition()} {
  socket_->connect(std::move(config.addr));

  canSend_->expression = [this]() {
    return (socket_->isConnected()) && (socket_->canWrite()->eval());
  };
  canReceive_->expression = [this]() {
    return (socket_->isConnected()) && (socket_->canRead()->eval());
  };
}

TCPCoreDataPipe::TCPCoreDataPipe(event::EventLoop& loop, ServerConfig config)
    : CoreDataPipe{}, role_{Role::SERVER}, server_{new networking::TCPServer{
                                               loop,
                                               networking::NetworkType::IPv4}},
      canSend_{loop.createComputedCondition()},
      canReceive_{loop.createComputedCondition()} {
  server_->bind(0);

  canSend_->expression = [this]() {
    return !!socket_ && socket_->canWrite()->eval();
  };

  canReceive_->expression = [this]() {
    return !!socket_ && socket_->canRead()->eval();
  };

  loop.arm("stun::TCPCoreDataPipe::serverCanAcceptTrigger",
           {server_->canAccept()}, [this]() {
             socket_ = std::unique_ptr<networking::TCPSocket>{
                 new networking::TCPSocket{server_->accept()}};
           });
}

/* virtual */ bool TCPCoreDataPipe::send(DataPacket packet) /* override */ {
  if (packetBytesSent_ == packetToSend_.size) {
    // Done with the current packet. Need to construct a new packet.
    assertTrue(packet.size < std::numeric_limits<uint16_t>::max(),
               "DataPacket too big for TCPCoreDataPipe.");
    MessageHeader header{static_cast<uint16_t>(packet.size)};

    header.serialize(packetToSend_.data);
    packetToSend_.size = MessageHeader::WireSize;
    packetToSend_.fill(packet.data, packet.size, sizeof(MessageHeader));
    packetBytesSent_ = 0;
  }

  size_t bytesLeft = packetToSend_.size - packetBytesSent_;
  size_t bytesSent =
      socket_->write(packetToSend_.data + packetBytesSent_, bytesLeft);
  packetBytesSent_ += bytesSent;

  // TODO: This should actually be supported
  assertTrue(packetBytesSent_ == packetToSend_.size,
             "TCPCoreDataPipe sent an incomplete packet.");

  return true;
}

/* virtual */ bool TCPCoreDataPipe::receive(DataPacket& output) /* override */ {
  // TODO: Receive more than 1 packet

  size_t bytesRead = socket_->read(receiveBuffer_.data + receiveBuffer_.size,
                                   ReceiveBufferSize - receiveBuffer_.size);
  receiveBuffer_.size += bytesRead;

  if (receiveBuffer_.size < sizeof(MessageHeader)) {
    return false;
  }

  auto header = MessageHeader::deserialize(receiveBuffer_.data);
  size_t fullPacketSize = sizeof(MessageHeader) + header.size;

  if (receiveBuffer_.size < fullPacketSize) {
    return false;
  }

  // TODO: Move memmove func into Packet
  output.fill(receiveBuffer_.data + sizeof(MessageHeader), header.size);
  memmove(receiveBuffer_.data, receiveBuffer_.data + fullPacketSize,
          receiveBuffer_.size - fullPacketSize);
  receiveBuffer_.size -= fullPacketSize;

  return true;
}

}; // namespace stun
