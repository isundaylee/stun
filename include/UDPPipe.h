#pragma once

#include <Pipe.h>
#include <SocketPipe.h>

#include <netdb.h>

#include <functional>

namespace stun {

struct UDPPacket: PipePacket {};

class UDPPipe: public SocketPipe<UDPPacket> {
public:
  UDPPipe() :
    SocketPipe(SocketType::UDP) {}

  UDPPipe(UDPPipe&& move) :
    SocketPipe(std::move(move)) {}
};

}
