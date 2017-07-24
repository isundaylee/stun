#pragma once

#include <networking/Pipe.h>
#include <networking/SocketPipe.h>

#include <netdb.h>

#include <functional>

namespace networking {

struct UDPPacket : PipePacket {};

class UDPPipe : public SocketPipe<UDPPacket> {
public:
  UDPPipe() : SocketPipe(SocketType::UDP) {}

  UDPPipe(UDPPipe&& move) : SocketPipe(std::move(move)) {}
};
}
