#pragma once

#include <crypto/AESEncryptor.h>
#include <crypto/Padder.h>
#include <event/Timer.h>
#include <networking/Packet.h>
#include <networking/PacketTranslator.h>
#include <networking/Tunnel.h>
#include <networking/UDPPipe.h>

using crypto::AESEncryptor;
using crypto::Padder;
using networking::Packet;
using networking::PacketTranslator;
using networking::TunnelPacket;
using networking::UDPPacket;

namespace stun {

static const size_t kDataPacketSize = 1 << 20;

class DataPacket : public Packet {
public:
  DataPacket() : Packet(kDataPacketSize) {}
};

class DataPipe {
public:
  DataPipe(networking::UDPPipe&& pipe, std::string const& aesKey,
           size_t minPaddingTo, event::Duration ttl);

  DataPipe(DataPipe&& move);

  void start();

  std::unique_ptr<event::FIFO<DataPacket>> inboundQ;
  std::unique_ptr<event::FIFO<DataPacket>> outboundQ;

  void setPrePrimed();
  event::Condition* isPrimed();
  event::Condition* didClose();

private:
  DataPipe(DataPipe const& copy) = delete;
  DataPipe& operator=(DataPipe const& copy) = delete;

  std::unique_ptr<networking::UDPPipe> pipe_;
  std::string aesKey_;
  size_t minPaddingTo_;
  std::unique_ptr<event::Timer> ttlTimer_;

  std::unique_ptr<event::Timer> probeTimer_;
  std::unique_ptr<event::Action> prober_;

  std::unique_ptr<AESEncryptor> aesEncryptor_;
  std::unique_ptr<Padder> padder_;
  std::unique_ptr<PacketTranslator<DataPacket, UDPPacket>> sender_;
  std::unique_ptr<PacketTranslator<UDPPacket, DataPacket>> receiver_;

  std::unique_ptr<event::BaseCondition> isPrimed_;

  void doProbe();
};
}
