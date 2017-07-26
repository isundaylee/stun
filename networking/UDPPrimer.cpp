#include "networking/UDPPrimer.h"

namespace networking {

UDPPrimer::UDPPrimer(UDPPipe& pipe)
    : inboundQ_(pipe.inboundQ.get()), outboundQ_(pipe.outboundQ.get()) {}

void UDPPrimer::start() {
  UDPPacket packet;
  packet.fill(kUDPPrimerContent);
  outboundQ_->push(packet);
}

event::Condition* UDPPrimer::didFinish() { return &didFinish_; }
}
