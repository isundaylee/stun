#include "stun/DataPipe.h"

namespace stun {

static const size_t kDataPipeFIFOSize = 50;

DataPipe::DataPipe(networking::UDPPipe&& pipe, std::string const& aesKey,
                   size_t minPaddingTo)
    : inboundQ(new event::FIFO<DataPacket>(kDataPipeFIFOSize)),
      outboundQ(new event::FIFO<DataPacket>(kDataPipeFIFOSize)),
      pipe_(std::move(pipe)), aesKey_(aesKey), minPaddingTo_(minPaddingTo),
      isPrimed_(new event::Condition()) {}

DataPipe::DataPipe(DataPipe&& move)
    : inboundQ(std::move(move.inboundQ)), outboundQ(std::move(move.outboundQ)),
      pipe_(std::move(move.pipe_)), aesKey_(std::move(move.aesKey_)),
      minPaddingTo_(move.minPaddingTo_),
      aesEncryptor_(std::move(move.aesEncryptor_)),
      padder_(std::move(move.padder_)), sender_(std::move(move.sender_)),
      receiver_(std::move(move.receiver_)),
      isPrimed_(std::move(move.isPrimed_)) {}

void DataPipe::setPrePrimed() { isPrimed_->fire(); }

event::Condition* DataPipe::isPrimed() { return isPrimed_.get(); }

void DataPipe::start() {
  // Prepare Encryptor-s
  if (minPaddingTo_ != 0) {
    padder_.reset(new crypto::Padder(minPaddingTo_));
  }
  aesEncryptor_.reset(new crypto::AESEncryptor(crypto::AESKey(aesKey_)));

  // Configure sender and receiver
  sender_.reset(new PacketTranslator<DataPacket, UDPPacket>(
      outboundQ.get(), pipe_.outboundQ.get()));
  sender_->transform = [this](DataPacket const& in) {
    UDPPacket out;
    out.fill(in.data, in.size);
    if (!!padder_) {
      out.size = padder_->encrypt(out.data, out.size, out.capacity);
    }
    out.size = aesEncryptor_->encrypt(out.data, out.size, out.capacity);
    return out;
  };
  sender_->start();

  receiver_.reset(new PacketTranslator<UDPPacket, DataPacket>(
      pipe_.inboundQ.get(), inboundQ.get()));
  receiver_->transform = [this](UDPPacket const& in) {
    isPrimed_->fire();

    DataPacket out;
    out.fill(in.data, in.size);
    out.size = aesEncryptor_->decrypt(out.data, out.size, out.capacity);
    if (!!padder_) {
      out.size = padder_->decrypt(out.data, out.size, out.capacity);
    }
    return out;
  };
  receiver_->start();

  // Prime the passive side's UDP connection if we're the active side
  if (*isPrimed_) {
    outboundQ->push(DataPacket());
  }
}
}
