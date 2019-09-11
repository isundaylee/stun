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
                   std::unique_ptr<networking::UDPSocket> socket, Config config)
    : inboundQ(new event::FIFO<DataPacket>(loop, kDataPipeFIFOSize)),
      outboundQ(new event::FIFO<DataPacket>(loop, kDataPipeFIFOSize)),
      loop_(loop), core_{loop, std::move(socket)}, config_{config},
      readyForSend_{loop.createBaseCondition()},
      didClose_(loop.createBaseCondition()) {
  // Sets up TTL killer
  if (config_.ttl != 0s) {
    ttlTimer_ = loop.createTimer(config_.ttl);
    ttlKiller_ = loop_.createAction({ttlTimer_->didFire()});
    ttlKiller_->callback.setMethod<DataPipe, &DataPipe::doKill>(this);
  }

  // Prepare Encryptor-s
  if (config_.minPaddingTo != 0) {
    padder_.reset(new crypto::Padder(config_.minPaddingTo));
  }

  if (config_.compression) {
    compressor_.reset(new crypto::LZOCompressor());
  }

  if (!config_.aesKey.empty()) {
    aesEncryptor_.reset(
        new crypto::AESEncryptor(crypto::AESKey(config_.aesKey)));
  }

  // Configure sender and receiver
  sender_ = loop_.createAction(
      {outboundQ->canPop(), core_.canSend(), readyForSend_.get()});
  sender_->callback.setMethod<DataPipe, &DataPipe::doSend>(this);
  receiver_ = loop_.createAction({inboundQ->canPush(), core_.canReceive()});
  receiver_->callback.setMethod<DataPipe, &DataPipe::doReceive>(this);

  // Setup prober
  probeTimer_ = loop_.createTimer(0s);
  prober_ = loop_.createAction(
      {probeTimer_->didFire(), outboundQ->canPush(), readyForSend_.get()});
  prober_->callback.setMethod<DataPipe, &DataPipe::doProbe>(this);
}

event::Condition* DataPipe::didClose() { return didClose_.get(); }

void DataPipe::doKill() {
  sender_.reset();
  receiver_.reset();
  prober_.reset();
  didClose_->fire();
}

void DataPipe::doProbe() {
  outboundQ->push(DataPacket());
  probeTimer_->reset(kDataPipeProbeInterval);
}

void DataPipe::doSend() {
  while (outboundQ->canPop()->eval()) {
    DataPacket data = outboundQ->pop();

    size_t payloadSize = data.size;

    if (!!compressor_) {
      data.size = compressor_->encrypt(data.data, data.size, data.capacity);
    }
    if (!!padder_) {
      data.size = padder_->encrypt(data.data, data.size, data.capacity);
    }
    if (!!aesEncryptor_) {
      data.size = aesEncryptor_->encrypt(data.data, data.size, data.capacity);
    }

    if (statEfficiency != nullptr) {
      statEfficiency->accumulate(payloadSize, data.size);
    }

    try {
      core_.send(std::move(data));
    } catch (networking::SocketClosedException const& ex) {
      // TODO: SocketClosedException should not leak outside of CoreDataPipe
      LOG_E("DataPipe") << "While sending: " << ex.what() << std::endl;
      doKill();
      return;
    }
  }
}

void DataPipe::doReceive() {
  while (inboundQ->canPush()->eval()) {
    DataPacket data;

    try {
      if (!core_.receive(data)) {
        break;
      }
    } catch (networking::SocketClosedException const& ex) {
      LOG_V("DataPipe") << "While receiving: " << ex.what() << std::endl;
      doKill();
      return;
    }

    size_t wireSize = data.size;

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

  readyForSend_->fire();
}
} // namespace stun
