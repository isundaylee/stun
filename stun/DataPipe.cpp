#include "stun/DataPipe.h"

#include <event/Trigger.h>

namespace stun {

static const event::Duration kDataPipeProbeInterval = 1000 /* ms */;
static const size_t kDataPipeFIFOSize = 256;

DataPipe::DataPipe(networking::UDPPipe&& pipe, std::string const& aesKey,
                   size_t minPaddingTo, event::Duration ttl)
    : inboundQ(new event::FIFO<DataPacket>(kDataPipeFIFOSize)),
      outboundQ(new event::FIFO<DataPacket>(kDataPipeFIFOSize)),
      pipe_(new networking::UDPPipe(std::move(pipe))), aesKey_(aesKey),
      minPaddingTo_(minPaddingTo), isPrimed_(new event::BaseCondition()) {
  if (ttl != 0) {
    ttlTimer_.reset(new event::Timer(ttl));
    networking::UDPPipe* pipe = this->pipe_.get();
    event::Trigger::arm({ttlTimer_->didFire()}, [pipe]() { pipe->close(); });
  }
}

DataPipe::DataPipe(DataPipe&& move)
    : inboundQ(std::move(move.inboundQ)), outboundQ(std::move(move.outboundQ)),
      pipe_(std::move(move.pipe_)), aesKey_(std::move(move.aesKey_)),
      minPaddingTo_(move.minPaddingTo_), ttlTimer_(std::move(move.ttlTimer_)),
      probeTimer_(std::move(move.probeTimer_)),
      prober_(std::move(move.prober_)),
      aesEncryptor_(std::move(move.aesEncryptor_)),
      padder_(std::move(move.padder_)), sender_(std::move(move.sender_)),
      receiver_(std::move(move.receiver_)),
      isPrimed_(std::move(move.isPrimed_)) {
  prober_->callback.target = this;
}

void DataPipe::setPrePrimed() { isPrimed_->fire(); }

event::Condition* DataPipe::isPrimed() { return isPrimed_.get(); }
event::Condition* DataPipe::didClose() { return pipe_->didClose(); }

void DataPipe::start() {
  // Prepare Encryptor-s
  if (minPaddingTo_ != 0) {
    padder_.reset(new crypto::Padder(minPaddingTo_));
  }
  aesEncryptor_.reset(new crypto::AESEncryptor(crypto::AESKey(aesKey_)));

  // Configure sender and receiver
  sender_.reset(new PacketTranslator<DataPacket, UDPPacket>(
      outboundQ.get(), pipe_->outboundQ.get()));
  sender_->transform = [this](DataPacket&& in) {
    UDPPacket out;
    out.fill(std::move(in));
    if (!!padder_) {
      out.size = padder_->encrypt(out.data, out.size, out.capacity);
    }
    out.size = aesEncryptor_->encrypt(out.data, out.size, out.capacity);
    return out;
  };
  sender_->start();

  receiver_.reset(new PacketTranslator<UDPPacket, DataPacket>(
      pipe_->inboundQ.get(), inboundQ.get()));
  receiver_->transform = [this](UDPPacket&& in) {
    isPrimed_->fire();

    DataPacket out;
    out.fill(std::move(in));
    out.size = aesEncryptor_->decrypt(out.data, out.size, out.capacity);
    if (!!padder_) {
      out.size = padder_->decrypt(out.data, out.size, out.capacity);
    }
    return out;
  };
  receiver_->start();

  // Setup prober
  probeTimer_.reset(new event::Timer(0));
  prober_.reset(new event::Action(
      {probeTimer_->didFire(), outboundQ->canPush(), isPrimed_.get()}));
  prober_->callback.setMethod<DataPipe, &DataPipe::doProbe>(this);
}

void DataPipe::doProbe() {
  outboundQ->push(DataPacket());
  probeTimer_->reset(kDataPipeProbeInterval);
}
}
