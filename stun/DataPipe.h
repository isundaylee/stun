#pragma once

#include <crypto/AESEncryptor.h>
#include <crypto/LZOCompressor.h>
#include <crypto/Padder.h>
#include <event/FIFO.h>
#include <event/Timer.h>
#include <networking/Packet.h>
#include <networking/Tunnel.h>
#include <networking/UDPSocket.h>
#include <stats/AvgStat.h>

using crypto::AESEncryptor;
using crypto::LZOCompressor;
using crypto::Padder;
using networking::Packet;
using networking::TunnelPacket;
using networking::UDPPacket;
using networking::UDPSocket;

namespace stun {

static const size_t kDataPacketSize = 1 << 20;

class DataPacket : public Packet {
public:
  DataPacket() : Packet(kDataPacketSize) {}
};

class DataPipe {
public:
  DataPipe(std::unique_ptr<UDPSocket> socket, std::string const& aesKey,
           size_t minPaddingTo, bool compression, event::Duration ttl);

  DataPipe(DataPipe&& move);

  std::unique_ptr<event::FIFO<DataPacket>> inboundQ;
  std::unique_ptr<event::FIFO<DataPacket>> outboundQ;

  void setPrePrimed();
  event::Condition* isPrimed();
  event::Condition* didClose();

private:
  DataPipe(DataPipe const& copy) = delete;
  DataPipe& operator=(DataPipe const& copy) = delete;

  // Settings & states
  std::unique_ptr<networking::UDPSocket> socket_;
  std::string aesKey_;
  size_t minPaddingTo_;

  std::unique_ptr<event::BaseCondition> didClose_;
  std::unique_ptr<event::BaseCondition> isPrimed_;

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

  std::unique_ptr<stats::AvgStat> statsEfficiency_;

  void doKill();
  void doProbe();
  void doSend();
  void doReceive();
};
}
