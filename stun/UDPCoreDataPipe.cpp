#include "stun/UDPCoreDataPipe.h"

namespace stun {

UDPCoreDataPipe::UDPCoreDataPipe(event::EventLoop& loop, ClientConfig config)
    : CoreDataPipe{}, socket_{
                          new networking::UDPSocket{loop, config.addr.type}} {
  socket_->connect(std::move(config.addr));
}

UDPCoreDataPipe::UDPCoreDataPipe(event::EventLoop& loop, ServerConfig config)
    : CoreDataPipe{}, socket_{new networking::UDPSocket{
                          loop, networking::NetworkType::IPv4}} {
  socket_->bind(0);
}

/* virtual */ bool UDPCoreDataPipe::send(DataPacket packet) /* override */ {
  if (!socket_->isConnected()) {
    return false;
  }

  networking::UDPPacket out;
  out.fill(packet.data, packet.size);
  socket_->write(std::move(out));
  return true;
}

/* virtual */ bool UDPCoreDataPipe::receive(DataPacket& output) /* override */ {
  networking::UDPPacket packet;
  bool read = socket_->read(packet);
  if (!read) {
    return false;
  }

  output.fill(packet.data, packet.size);
  return true;
}

}; // namespace stun