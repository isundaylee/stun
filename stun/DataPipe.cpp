#include "stun/DataPipe.h"

#include <event/Trigger.h>

namespace stun {

static const event::Duration kDataPipeProbeInterval = 1000 /* ms */;
static const size_t kDataPipeFIFOSize = 256;

DataPipe::DataPipe(std::unique_ptr<networking::UDPSocket> socket,
                   std::string const& aesKey, size_t minPaddingTo,
                   event::Duration ttl)
    : inboundQ(new event::FIFO<DataPacket>(kDataPipeFIFOSize)),
      outboundQ(new event::FIFO<DataPacket>(kDataPipeFIFOSize)),
      socket_(std::move(socket)), aesKey_(aesKey), minPaddingTo_(minPaddingTo),
      didClose_(new event::BaseCondition()),
      isPrimed_(new event::BaseCondition()) {
  if (ttl != 0) {
    ttlTimer_.reset(new event::Timer(ttl));
    ttlKiller_.reset(new event::Action({ttlTimer_->didFire()}));
    ttlKiller_->callback.setMethod<DataPipe, &DataPipe::doKill>(this);
  }
}

DataPipe::DataPipe(DataPipe&& move)
    : inboundQ(std::move(move.inboundQ)), outboundQ(std::move(move.outboundQ)),
      socket_(std::move(move.socket_)), aesKey_(std::move(move.aesKey_)),
      minPaddingTo_(move.minPaddingTo_), didClose_(std::move(move.didClose_)),
      isPrimed_(std::move(move.isPrimed_)),
      ttlTimer_(std::move(move.ttlTimer_)),
      probeTimer_(std::move(move.probeTimer_)),
      prober_(std::move(move.prober_)),
      aesEncryptor_(std::move(move.aesEncryptor_)),
      padder_(std::move(move.padder_)), sender_(std::move(move.sender_)),
      receiver_(std::move(move.receiver_)) {
  prober_->callback.target = this;
}

void DataPipe::setPrePrimed() { isPrimed_->fire(); }

event::Condition* DataPipe::didClose() { return didClose_.get(); }
event::Condition* DataPipe::isPrimed() { return isPrimed_.get(); }

void DataPipe::start() {
  // Prepare Encryptor-s
  if (minPaddingTo_ != 0) {
    padder_.reset(new crypto::Padder(minPaddingTo_));
  }
  aesEncryptor_.reset(new crypto::AESEncryptor(crypto::AESKey(aesKey_)));

  // Configure sender and receiver
  sender_.reset(new event::Action(
      {outboundQ->canPop(), socket_->canWrite(), isPrimed_.get()}));
  sender_->callback.setMethod<DataPipe, &DataPipe::doSend>(this);
  receiver_.reset(new event::Action({inboundQ->canPush(), socket_->canRead()}));
  receiver_->callback.setMethod<DataPipe, &DataPipe::doReceive>(this);

  // Setup prober
  probeTimer_.reset(new event::Timer(0));
  prober_.reset(new event::Action(
      {probeTimer_->didFire(), outboundQ->canPush(), isPrimed_.get()}));
  prober_->callback.setMethod<DataPipe, &DataPipe::doProbe>(this);
}

void DataPipe::stop() {
  sender_.reset();
  receiver_.reset();
  prober_.reset();
}

void DataPipe::doKill() {
  stop();
  socket_.reset();
  didClose_->fire();
}

void DataPipe::doProbe() {
  outboundQ->push(DataPacket());
  probeTimer_->reset(kDataPipeProbeInterval);
}

void DataPipe::doSend() {
  DataPacket data = outboundQ->pop();
  UDPPacket out;

  out.fill(std::move(data));
  if (!!padder_) {
    out.size = padder_->encrypt(out.data, out.size, out.capacity);
  }
  out.size = aesEncryptor_->encrypt(out.data, out.size, out.capacity);

  socket_->write(std::move(out));
}

void DataPipe::doReceive() {
  UDPPacket in;
  assertTrue(socket_->read(in), "Am I being lied to by socket_->canRead()?");
  DataPacket data;

  data.fill(std::move(in));
  data.size = aesEncryptor_->decrypt(data.data, data.size, data.capacity);
  if (!!padder_) {
    data.size = padder_->decrypt(data.data, data.size, data.capacity);
  }

  if (data.size > 0) {
    inboundQ->push(std::move(data));
  }

  isPrimed_->fire();
}
}
