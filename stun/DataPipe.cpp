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

namespace {

class CoreDataPipeFactory {
public:
  CoreDataPipeFactory(event::EventLoop& loop) : loop_{loop} {}

  auto operator()(UDPCoreDataPipe::ServerConfig const& config) {
    return std::make_unique<UDPCoreDataPipe>(loop_, config);
  }

  auto operator()(UDPCoreDataPipe::ClientConfig const& config) {
    return std::make_unique<UDPCoreDataPipe>(loop_, config);
  }

private:
  event::EventLoop& loop_;
};

}; // namespace

DataPipe::DataPipe(event::EventLoop& loop, Config config)
    : inboundQ(new event::FIFO<DataPacket>(loop, kDataPipeFIFOSize)),
      outboundQ(new event::FIFO<DataPacket>(loop, kDataPipeFIFOSize)),
      loop_(loop), config_{config}, didClose_(loop.createBaseCondition()) {

  core_ = std::visit(CoreDataPipeFactory{loop_}, config.core);

  // Sets up TTL killer
  if (config_.common.ttl != 0s) {
    ttlTimer_ = loop.createTimer(config_.common.ttl);
    ttlKiller_ = loop_.createAction({ttlTimer_->didFire()});
    ttlKiller_->callback.setMethod<DataPipe, &DataPipe::doKill>(this);
  }

  // Prepare Encryptor-s
  if (config_.common.minPaddingTo != 0) {
    padder_.reset(new crypto::Padder(config_.common.minPaddingTo));
  }

  if (config_.common.compression) {
    compressor_.reset(new crypto::LZOCompressor());
  }

  if (!config_.common.aesKey.empty()) {
    aesEncryptor_.reset(
        new crypto::AESEncryptor(crypto::AESKey(config_.common.aesKey)));
  }

  // Configure sender and receiver
  sender_ = loop_.createAction({outboundQ->canPop(), core_->canSend()});
  sender_->callback.setMethod<DataPipe, &DataPipe::doSend>(this);
  receiver_ = loop_.createAction({inboundQ->canPush(), core_->canReceive()});
  receiver_->callback.setMethod<DataPipe, &DataPipe::doReceive>(this);

  // Setup prober
  probeTimer_ = loop_.createTimer(0s);
  prober_ = loop_.createAction({probeTimer_->didFire(), outboundQ->canPush()});
  prober_->callback.setMethod<DataPipe, &DataPipe::doProbe>(this);
} // namespace stun

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
      if (!core_->send(std::move(data))) {
        LOG_E("DataPipe") << "Dropped a packet due to send() failure."
                          << std::endl;
      }
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
      if (!core_->receive(data)) {
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
}
} // namespace stun
