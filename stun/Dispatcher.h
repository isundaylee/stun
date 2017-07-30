#pragma once

#include <stun/DataPipe.h>

#include <networking/Tunnel.h>
#include <networking/UDPPipe.h>

namespace stun {

using networking::PacketTranslator;
using networking::TunnelPacket;

class Dispatcher {
public:
  Dispatcher(networking::Tunnel&& tunnel);

  void start();
  void addDataPipe(DataPipe* dataPipe);

private:
  Dispatcher(Dispatcher const& copy) = delete;
  Dispatcher& operator=(Dispatcher const& copy) = delete;

  Dispatcher(Dispatcher&& move) = delete;
  Dispatcher& operator=(Dispatcher&& move) = delete;

  networking::Tunnel tunnel_;
  std::unique_ptr<DataPipe> dataPipe_;

  std::unique_ptr<event::Action> sender_;
  std::unique_ptr<event::Action> receiver_;

  void doSend();
  void doReceive();
};
}
