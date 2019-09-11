#include "stun/UDPCoreDataPipe.h"

namespace stun {

UDPCoreDataPipe::UDPCoreDataPipe(event::EventLoop& loop,
                                 std::unique_ptr<networking::UDPSocket> socket)
    : loop_{loop}, socket_{std::move(socket)} {}

bool UDPCoreDataPipe::send(DataPacket packet) {
  if (!socket_->isConnected()) {
    return false;
  }

  networking::UDPPacket out;
  out.fill(packet.data, packet.size);
  socket_->write(std::move(out));
  return true;
}

bool UDPCoreDataPipe::receive(DataPacket& output) {
  networking::UDPPacket packet;
  bool read = socket_->read(packet);
  if (!read) {
    return false;
  }

  output.fill(packet.data, packet.size);
  return true;
}

}; // namespace stun