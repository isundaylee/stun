#pragma once

#include <event/Condition.h>
#include <event/Timer.h>
#include <networking/UDPPipe.h>

#include <memory>

namespace networking {

const std::string kUDPPrimerContent = "primer";
const event::Duration kUDPPrimerInterval = 1000;


class UDPPrimer {
public:
  UDPPrimer(UDPPipe& pipe);

  void start();

private:
  UDPPrimer(UDPPrimer const& copy) = delete;
  UDPPrimer& operator=(UDPPrimer const& copy) = delete;

  UDPPrimer(UDPPrimer&& move) = delete;
  UDPPrimer& operator=(UDPPrimer&& move) = delete;

  event::FIFO<UDPPacket>* outboundQ_;
  std::unique_ptr<event::Timer> timer_;
  std::unique_ptr<event::Action> action_;
};

class UDPPrimerAcceptor {
public:
  UDPPrimerAcceptor(UDPPipe &pipe);

  void start();
  event::Condition* didFinish();

private:
  UDPPrimerAcceptor(UDPPrimerAcceptor const& copy) = delete;
  UDPPrimerAcceptor& operator=(UDPPrimerAcceptor const& copy) = delete;

  UDPPrimerAcceptor(UDPPrimerAcceptor&& move) = delete;
  UDPPrimerAcceptor& operator=(UDPPrimerAcceptor&& move) = delete;

  event::FIFO<UDPPacket>* inboundQ_;
  event::Condition didFinish_;
  std::unique_ptr<event::Action> listener_;
};
}
