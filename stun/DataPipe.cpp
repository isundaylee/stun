#include "stun/DataPipe.h"

#include <event/Trigger.h>

#include <chrono>

namespace stun {

using namespace std::chrono_literals;

static const event::Duration kDataPipeProbeInterval = 1s;

#if TARGET_IOS
static const size_t kDataPipeFIFOSize = 16;
#else
static const size_t kDataPipeFIFOSize = 256;
#endif

DataPipe::DataPipe(event::EventLoop& loop,
                   std::unique_ptr<networking::UDPSocket> socket,
                   std::string const& aesKey, size_t minPaddingTo,
                   bool compression, event::Duration ttl)
    : inboundQ(new event::FIFO<DataPacket>(loop, kDataPipeFIFOSize)),
      outboundQ(new event::FIFO<DataPacket>(loop, kDataPipeFIFOSize)),
      loop_(loop), socket_(std::move(socket)), aesKey_(aesKey),
      minPaddingTo_(minPaddingTo), didClose_(loop.createBaseCondition()),
      isPrimed_(loop.createBaseCondition()) {
  // Sets up TTL killer
  if (ttl != 0s) {
    ttlTimer_ = loop.createTimer(ttl);
    ttlKiller_ = loop_.createAction({ttlTimer_->didFire()});
    ttlKiller_->callback.setMethod<DataPipe, &DataPipe::doKill>(this);
  }

  // Prepare Encryptor-s
  if (minPaddingTo_ != 0) {
    padder_.reset(new crypto::Padder(minPaddingTo_));
  }

  if (compression) {
    compressor_.reset(new crypto::LZOCompressor());
  }

  if (!aesKey_.empty()) {
    aesEncryptor_.reset(new crypto::AESEncryptor(crypto::AESKey(aesKey_)));
  }

  // Configure sender and receiver
  sender_ = loop_.createAction(
      {outboundQ->canPop(), socket_->canWrite(), isPrimed_.get()});
  sender_->callback.setMethod<DataPipe, &DataPipe::doSend>(this);
  receiver_ = loop_.createAction({inboundQ->canPush(), socket_->canRead()});
  receiver_->callback.setMethod<DataPipe, &DataPipe::doReceive>(this);

  // Setup prober
  probeTimer_ = loop_.createTimer(0s);
  prober_ = loop_.createAction(
      {probeTimer_->didFire(), outboundQ->canPush(), isPrimed_.get()});
  prober_->callback.setMethod<DataPipe, &DataPipe::doProbe>(this);
}

DataPipe::DataPipe(DataPipe&& move)
    : inboundQ(std::move(move.inboundQ)), outboundQ(std::move(move.outboundQ)),
      statEfficiency(move.statEfficiency), loop_(move.loop_),
      socket_(std::move(move.socket_)), aesKey_(std::move(move.aesKey_)),
      minPaddingTo_(move.minPaddingTo_), didClose_(std::move(move.didClose_)),
      isPrimed_(std::move(move.isPrimed_)),
      ttlTimer_(std::move(move.ttlTimer_)),
      probeTimer_(std::move(move.probeTimer_)),
      prober_(std::move(move.prober_)),
      compressor_(std::move(move.compressor_)),
      padder_(std::move(move.padder_)),
      aesEncryptor_(std::move(move.aesEncryptor_)),
      sender_(std::move(move.sender_)), receiver_(std::move(move.receiver_)) {
  ttlKiller_->callback.target = this;
  prober_->callback.target = this;
  sender_->callback.target = this;
  receiver_->callback.target = this;
}

void DataPipe::setPrePrimed() { isPrimed_->fire(); }

event::Condition* DataPipe::didClose() { return didClose_.get(); }
event::Condition* DataPipe::isPrimed() { return isPrimed_.get(); }

void DataPipe::doKill() {
  sender_.reset();
  receiver_.reset();
  prober_.reset();
  socket_.reset();
  didClose_->fire();
}

void DataPipe::doProbe() {
  outboundQ->push(DataPacket());
  probeTimer_->reset(kDataPipeProbeInterval);
}

void DataPipe::doSend() {
  while (outboundQ->canPop()->eval()) {
    DataPacket data = outboundQ->pop();
    UDPPacket out;

    size_t payloadSize = data.size;

    out.fill(std::move(data));
    if (!!compressor_) {
      out.size = compressor_->encrypt(out.data, out.size, out.capacity);
    }
    if (!!padder_) {
      out.size = padder_->encrypt(out.data, out.size, out.capacity);
    }
    if (!!aesEncryptor_) {
      out.size = aesEncryptor_->encrypt(out.data, out.size, out.capacity);
    }

    if (statEfficiency != nullptr) {
      statEfficiency->accumulate(payloadSize, out.size);
    }

    try {
      socket_->write(std::move(out));
    } catch (networking::SocketClosedException const& ex) {
      LOG_V("DataPipe") << "While sending: " << ex.what() << std::endl;
      doKill();
      return;
    }
  }
}

void DataPipe::doReceive() {
  while (inboundQ->canPush()->eval()) {
    UDPPacket in;
    DataPacket data;

    try {
      bool read = socket_->read(in);
      if (!read) {
        break;
      }
    } catch (networking::SocketClosedException const& ex) {
      LOG_V("DataPipe") << "While receiving: " << ex.what() << std::endl;
      doKill();
      return;
    }

    size_t wireSize = in.size;

    data.fill(std::move(in));
    if (!!aesEncryptor_) {
      data.size = aesEncryptor_->decrypt(data.data, data.size, data.capacity);
    }
    if (!!padder_) {
      data.size = padder_->decrypt(data.data, data.size, data.capacity);
    }
    if (!!compressor_) {
      data.size = compressor_->decrypt(data.data, data.size, data.capacity);
    }

    if (statEfficiency != nullptr) {
      statEfficiency->accumulate(data.size, wireSize);
    }

    if (data.size > 0) {
      inboundQ->push(std::move(data));
    }
  }

  isPrimed_->fire();
}
} // namespace stun
