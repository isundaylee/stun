#pragma once

#include <event/Condition.h>
#include <networking/UDPPipe.h>

#include <memory>

namespace networking {

const std::string kUDPPrimerContent = "primer";
const std::string kUDPPrimerAckContent = "primer_ack";

class UDPPrimer {
public:
  UDPPrimer(UDPPipe& pipe);

  void start();
  event::Condition* didFinish();

private:
  UDPPrimer(UDPPrimer const& copy) = delete;
  UDPPrimer& operator=(UDPPrimer const& copy) = delete;

  UDPPrimer(UDPPrimer&& move) = delete;
  UDPPrimer& operator=(UDPPrimer&& move) = delete;

  event::FIFO<UDPPacket>* inboundQ_;
  event::FIFO<UDPPacket>* outboundQ_;
  event::Condition didFinish_;
};
}
