#include "networking/Messenger.h"

#include <event/Trigger.h>

namespace networking {

typedef uint32_t MessengerLengthHeaderType;

static const std::string kMessengerHeartBeatMessageType = "heartbeat";
static const event::Duration kMessengerHeartBeatInterval = 1000 /* ms */;
static const event::Duration kMessengerHeartBeatTimeout = 10000 /* ms */;
static const size_t kMessengerOutboundQueueSize = 32;

Messenger::Messenger(std::unique_ptr<TCPSocket> socket)
    : outboundQ(new event::FIFO<Message>(kMessengerOutboundQueueSize)),
      socket_(std::move(socket)), bufferUsed_(0),
      didDisconnect_(new event::BaseCondition()) {}

void Messenger::start() {
  sender_.reset(new event::Action({socket_->canWrite(), outboundQ->canPop()}));
  sender_->callback.setMethod<Messenger, &Messenger::doSend>(this);
  receiver_.reset(
      new event::Action({socket_->canRead(), outboundQ->canPush()}));
  receiver_->callback.setMethod<Messenger, &Messenger::doReceive>(this);

  heartbeatTimer_.reset(new event::Timer(0));
  heartbeatSender_.reset(
      new event::Action({heartbeatTimer_->didFire(), outboundQ->canPush()}));
  heartbeatSender_->callback = [this]() {
    outboundQ->push(Message(kMessengerHeartBeatMessageType, ""));
    heartbeatTimer_->extend(kMessengerHeartBeatInterval);
  };

  heartbeatMissedTimer_.reset(new event::Timer(kMessengerHeartBeatTimeout));
  event::Trigger::arm({heartbeatMissedTimer_->didFire()},
                      [this]() {
                        LOG_T("Messenger")
                            << "Disconnected due to missed heartbeats."
                            << std::endl;
                        disconnect();
                      });
}

void Messenger::disconnect() {
  sender_.reset();
  receiver_.reset();
  heartbeatTimer_.reset();
  heartbeatSender_.reset();
  heartbeatMissedTimer_.reset();
  socket_.reset();
  didDisconnect_->fire();
}

void Messenger::doReceive() {
  try {
    size_t read = socket_->read(buffer_ + bufferUsed_,
                                kMessengerReceiveBufferSize - bufferUsed_);
    bufferUsed_ += read;
  } catch (SocketClosedException const& ex) {
    LOG_T("Messenger") << "Disconnected while receiving. Reason: " << ex.what()
                       << std::endl;
    disconnect();
    return;
  }

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

    MessengerLengthHeaderType payloadLen = messageLen;
    for (auto decryptor = encryptors_.rbegin(); decryptor != encryptors_.rend();
         decryptor++) {
      payloadLen =
          (*decryptor)->decrypt(message.data, payloadLen, kMessageSize);
    }
    message.size = payloadLen;

    if (bufferUsed_ > totalLen) {
      // Move the left-over to the front
      memmove(buffer_, buffer_ + totalLen, bufferUsed_ - (totalLen));
    }

    if (!message.isValid()) {
      LOG_T("Messenger") << "Disconnected due to invalid message." << std::endl;
      disconnect();
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
        outboundQ->push(std::move(response));
      }
    }
  }
}

void Messenger::doSend() {
  Message message = outboundQ->pop();

  if (message.getType() != kMessengerHeartBeatMessageType) {
    LOG_T("Messenger") << "Sent: " << message.getType() << " = "
                       << message.getBody() << std::endl;
  }

  MessengerLengthHeaderType payloadSize = message.size;
  for (auto const& encryptor : encryptors_) {
    payloadSize =
        encryptor->encrypt(message.data, payloadSize, message.capacity);
  }

  try {
    int written = socket_->write((Byte*)&payloadSize, sizeof(payloadSize));
    assertTrue(written == sizeof(payloadSize),
               "Message length header fragmented");
    written = socket_->write(message.data, payloadSize);
    assertTrue(written == payloadSize, "Message content fragmented");
  } catch (SocketClosedException const& ex) {
    LOG_T("Messenger") << "Disconnected while sending. Reason: " << ex.what()
                       << std::endl;
    disconnect();
  }
}

void Messenger::addEncryptor(crypto::Encryptor* encryptor) {
  encryptors_.emplace_back(encryptor);
}

event::Condition* Messenger::didDisconnect() const {
  return didDisconnect_.get();
}
}
