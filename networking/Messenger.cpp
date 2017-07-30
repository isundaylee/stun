#include "networking/Messenger.h"

namespace networking {

typedef uint32_t MessengerLengthHeaderType;

Messenger::Messenger(TCPPipe& client)
    : client_(client), inboundQ_(client.inboundQ.get()),
      outboundQ_(client.outboundQ.get()), bufferUsed_(0),
      didReceiveInvalidMessage_(new event::Condition()) {}

void Messenger::start() {
  receiver_.reset(
      new event::Action({inboundQ_->canPop(), outboundQ_->canPush()}));
  receiver_->callback.setMethod<Messenger, &Messenger::doReceive>(this);
}

void Messenger::doReceive() {
  while (*inboundQ_->canPop()) {
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
      LOG() << "Messenger received: " << message.getType() << " - "
            << message.getBody() << std::endl;

      Message response = handler(message);
      if (response.size > 0) {
        send(response);
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

  client_.outboundQ->push(packet);
  LOG() << "Messenger sent: " << message.getType() << " - " << message.getBody()
        << std::endl;
}

void Messenger::addEncryptor(crypto::Encryptor* encryptor) {
  encryptors_.emplace_back(encryptor);
}

event::Condition* Messenger::didReceiveInvalidMessage() const {
  return didReceiveInvalidMessage_.get();
}
}
