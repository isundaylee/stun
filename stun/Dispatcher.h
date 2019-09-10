#pragma once

#include <stun/DataPipe.h>

#include <networking/Tunnel.h>
#include <stats/CountStat.h>
#include <stats/RateStat.h>
#include <stats/RatioStat.h>

namespace stun {

using networking::TunnelPacket;

class Dispatcher {
public:
  Dispatcher(event::EventLoop& loop,
             std::unique_ptr<networking::Tunnel> tunnel);

  size_t bytesDispatched = 0;

  void addDataPipe(std::unique_ptr<DataPipe> dataPipe);

private:
  Dispatcher(Dispatcher const& copy) = delete;
  Dispatcher& operator=(Dispatcher const& copy) = delete;

  Dispatcher(Dispatcher&& move) = delete;
  Dispatcher& operator=(Dispatcher&& move) = delete;

  event::EventLoop& loop_;

  std::unique_ptr<networking::Tunnel> tunnel_;
  std::vector<std::unique_ptr<DataPipe>> dataPipes_;
  size_t currentDataPipeIndex_;

  std::unique_ptr<event::ComputedCondition> canSend_;
  std::unique_ptr<event::ComputedCondition> canReceive_;

  std::unique_ptr<event::Action> sender_;
  std::unique_ptr<event::Action> receiver_;

  stats::CountStat statTxPackets_;
  stats::RateStat statTxBytes_;
  stats::CountStat statRxPackets_;
  stats::RateStat statRxBytes_;
  stats::RatioStat statEfficiency_;

  void doSend();
  void doReceive();

  bool calculateCanReceive();
  bool calculateCanSend();
};
} // namespace stun
