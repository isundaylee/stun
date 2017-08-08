#pragma once

#include <stun/DataPipe.h>

#include <networking/Tunnel.h>
#include <stats/RateStat.h>

namespace stun {

using networking::TunnelPacket;

class Dispatcher {
public:
  Dispatcher(networking::Tunnel&& tunnel);

  size_t bytesDispatched = 0;

  void addDataPipe(std::unique_ptr<DataPipe> dataPipe);

private:
  Dispatcher(Dispatcher const& copy) = delete;
  Dispatcher& operator=(Dispatcher const& copy) = delete;

  Dispatcher(Dispatcher&& move) = delete;
  Dispatcher& operator=(Dispatcher&& move) = delete;

  networking::Tunnel tunnel_;
  std::vector<std::unique_ptr<DataPipe>> dataPipes_;
  size_t currentDataPipeIndex_;

  std::unique_ptr<event::ComputedCondition> canSend_;
  std::unique_ptr<event::ComputedCondition> canReceive_;

  std::unique_ptr<event::Action> sender_;
  std::unique_ptr<event::Action> receiver_;

  stats::RateStat<size_t> statTxBytes_;
  stats::RateStat<size_t> statRxBytes_;

  void doSend();
  void doReceive();

  bool calculateCanReceive();
  bool calculateCanSend();
};
}
