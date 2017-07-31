#include "networking/Messenger.h"

#include <event/Trigger.h>

namespace networking {

typedef uint32_t MessengerLengthHeaderType;

static const std::string kMessengerHeartBeatMessageType = "heartbeat";
static const event::Duration kMessengerHeartBeatInterval = 1000;
static const event::Duration kMessengerHeartBeatTimeout = 10000;

Messenger::Messenger(TCPPipe& client)
    : client_(client), inboundQ_(client.inboundQ.get()),
      outboundQ_(client.outboundQ.get()), bufferUsed_(0),
      didReceiveInvalidMessage_(new event::BaseCondition()),
      didMissHeartbeat_(new event::BaseCondition()) {}

void Messenger::start() {
  receiver_.reset(
      new event::Action({inboundQ_->canPop(), outboundQ_->canPush()}));
  receiver_->callback.setMethod<Messenger, &Messenger::doReceive>(this);

  heartbeatTimer_.reset(new event::Timer(0));
  heartbeatSender_.reset(
      new event::Action({heartbeatTimer_->didFire(), canSend()}));
  heartbeatSender_->callback = [this]() {
    send(Message(kMessengerHeartBeatMessageType, ""));
    heartbeatTimer_->extend(kMessengerHeartBeatInterval);
  };

  heartbeatMissedTimer_.reset(new event::Timer(kMessengerHeartBeatTimeout));
  event::Trigger::arm({heartbeatMissedTimer_->didFire()},
                      [this]() { didMissHeartbeat_->fire(); });
}

void Messenger::doReceive() {
  while (inboundQ_->canPop()->eval()) {
    TCPPacket in = inboundQ_->pop();

    // Append the new content to our buffer.
    assertTrue(bufferUsed_ + in.size <= kMessengerReceiveBufferSize,
               "Messenger receive buffer overflow.");
    memcpy(buffer_ + bufferUsed_, in.data, in.size);
    bufferUsed_ += in.size;

    // Deliver complete messages
    while (bufferUsed_ >= sizeof(MessengerLengthHeaderType)) {
      int messageLen = *((MessengerLengthHeaderType*)buffer_);
      int totalLen = sizeof(MessengerLengthHeaderType) + messageLen;
      if (bufferUsed_ < totalLen) {
        break;
      }

      // We have a complete message
      Message message;
      message.fill(buffer_ + sizeof(MessengerLengthHeaderType), messageLen);

      size_t payloadLen = messageLen;
      for (auto decryptor = encryptors_.rbegin();
           decryptor != encryptors_.rend(); decryptor++) {
        payloadLen =
            (*decryptor)->decrypt(message.data, payloadLen, kMessageSize);
      }
      message.size = payloadLen;

      if (bufferUsed_ > totalLen) {
        // Move the left-over to the front
        memmove(buffer_, buffer_ + totalLen, bufferUsed_ - (totalLen));
      }

      if (!message.isValid()) {
        didReceiveInvalidMessage_->fire();

        // We return here because it's unlikely we'll get valid messages, and
        // more likely than not we'll be gone after the onInvalidMessage
        // callback
        // fires.
        return;
      }

      bufferUsed_ -= (totalLen);

      if (message.getType() != kMessengerHeartBeatMessageType) {
        LOG_T("Messenger") << "Received: " << message.getType() << " - "
                           << message.getBody() << std::endl;
      }

      if (message.getType() == kMessengerHeartBeatMessageType) {
        heartbeatMissedTimer_->reset(kMessengerHeartBeatTimeout);
      } else {
        Message response = handler(message);
        if (response.size > 0) {
          send(response);
        }
      }
    }
  }
}

event::Condition* Messenger::canSend() { return outboundQ_->canPush(); }

void Messenger::send(Message const& message) {
  // TODO: Think about large messages later
  assertTrue(message.size + sizeof(MessengerLengthHeaderType) <= kTCPPacketSize,
             "Message too large");

  TCPPacket packet;
  Byte* payload = packet.data + sizeof(MessengerLengthHeaderType);
  memcpy(payload, message.data, message.size);

  size_t payloadSize = message.size;
  for (auto const& encryptor : encryptors_) {
    payloadSize = encryptor->encrypt(
        payload, payloadSize, kMessageSize - sizeof(MessengerLengthHeaderType));
  }

  packet.size = sizeof(MessengerLengthHeaderType) + payloadSize;
  *((MessengerLengthHeaderType*)packet.data) = payloadSize;

  client_.outboundQ->push(std::move(packet));

  if (message.getType() != kMessengerHeartBeatMessageType) {
    LOG_T("Messenger") << "Sent: " << message.getType() << " = "
                       << message.getBody() << std::endl;
  }
}

void Messenger::addEncryptor(crypto::Encryptor* encryptor) {
  encryptors_.emplace_back(encryptor);
}

event::Condition* Messenger::didReceiveInvalidMessage() const {
  return didReceiveInvalidMessage_.get();
}

event::Condition* Messenger::didMissHeartbeat() const {
  return didMissHeartbeat_.get();
}
}
