#pragma once

#include <networking/Packet.h>
#include <networking/Pipe.h>
#include <networking/SocketPipe.h>

#include <netdb.h>

#include <functional>

namespace networking {

const size_t kUDPPacketSize = 2048;

struct UDPPacket : Packet<kUDPPacketSize> {};

class UDPPipe : public SocketPipe<UDPPacket> {
public:
  UDPPipe() : SocketPipe(SocketType::UDP) {}

  UDPPipe(UDPPipe&& move) : SocketPipe(std::move(move)) {}
};
}
