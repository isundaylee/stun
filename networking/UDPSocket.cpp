#include "networking/UDPSocket.h"

namespace networking {

void UDPSocket::write(UDPPacket packet) {
  size_t written = Socket::write(packet.data, packet.size);

  if (written < packet.size) {
    logger_.withLogLevel(common::LogLevel::VERBOSE)
        << "A UDPPacket is fragmented." << std::endl;
  }
}

bool UDPSocket::read(UDPPacket& packet) {
  size_t read = Socket::read(packet.data, packet.capacity);
  assertTrue(read < packet.capacity, "UDPPacket size too small.");
  packet.size = read;

  return (read > 0);
}
} // namespace networking
