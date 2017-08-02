#include "networking/UDPSocket.h"

namespace networking {

void UDPSocket::write(UDPPacket packet) {
  size_t written = Socket::write(packet.data, packet.size);
  assertTrue(written == packet.size, "UDPPacket fragmented.");
}

bool UDPSocket::read(UDPPacket& packet) {
  size_t read = Socket::read(packet.data, packet.capacity);
  assertTrue(read < packet.capacity, "UDPPacket size too small.");
  packet.size = read;

  return (read > 0);
}
}
