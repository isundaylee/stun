#pragma once

#include <variant>

#include <stun/UDPCoreDataPipe.h>

#include <crypto/AESEncryptor.h>
#include <crypto/LZOCompressor.h>
#include <crypto/Padder.h>
#include <event/FIFO.h>
#include <event/Timer.h>
#include <networking/Packet.h>
#include <networking/Tunnel.h>
#include <networking/UDPSocket.h>
#include <stats/RatioStat.h>

using crypto::AESEncryptor;
using crypto::LZOCompressor;
using crypto::Padder;
using networking::Packet;
using networking::TunnelPacket;
using networking::UDPPacket;
using networking::UDPSocket;

namespace stun {
class DataPipe {
public:
  struct CommonConfig {
    std::string aesKey;
    size_t minPaddingTo;
    bool compression;
    event::Duration ttl;
  };

  using CoreConfig = std::variant<UDPCoreDataPipe::ClientConfig,
                                  UDPCoreDataPipe::ServerConfig>;

  struct Config {
    CoreConfig core;
    CommonConfig common;
  };

  DataPipe(event::EventLoop& loop, Config config);

  DataPipe(DataPipe const& copy) = delete;
  DataPipe& operator=(DataPipe const& copy) = delete;

  DataPipe(DataPipe&& copy) = delete;
  DataPipe& operator=(DataPipe&& copy) = delete;

  std::unique_ptr<event::FIFO<DataPacket>> inboundQ;
  std::unique_ptr<event::FIFO<DataPacket>> outboundQ;

  event::Condition* didClose();

  stats::RatioStat* statEfficiency;

  UDPCoreDataPipe& getCore() { return *core_; }

private:
  event::EventLoop& loop_;

  std::unique_ptr<UDPCoreDataPipe> core_;

  // Settings & states
  Config config_;

  std::unique_ptr<event::BaseCondition> didClose_;

  // TTL
  std::unique_ptr<event::Timer> ttlTimer_;
  std::unique_ptr<event::Action> ttlKiller_;

  // Probe
  std::unique_ptr<event::Timer> probeTimer_;
  std::unique_ptr<event::Action> prober_;

  // Data channel
  std::unique_ptr<LZOCompressor> compressor_;
  std::unique_ptr<Padder> padder_;
  std::unique_ptr<AESEncryptor> aesEncryptor_;

  std::unique_ptr<event::Action> sender_;
  std::unique_ptr<event::Action> receiver_;

  void doKill();
  void doProbe();
  void doSend();
  void doReceive();
};
} // namespace stun
