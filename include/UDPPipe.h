#pragma once

#include <FIFO.h>
#include <Pipe.h>
#include <SocketPipe.h>

#include <ev/ev++.h>
#include <netdb.h>

#include <functional>

namespace stun {

const int kUDPPacketBufferSize = 4096;
const int kUDPInboundQueueSize = 32;
const int kUDPOutboundQueueSize = 32;

struct UDPPacket: PipePacket {};

class UDPPipe: public SocketPipe<UDPPacket> {
public:
  UDPPipe() :
    SocketPipe(SocketType::UDP) {}
};

}
