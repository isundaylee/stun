#include "networking/Messenger.h"

#include <event/Trigger.h>

namespace networking {

typedef uint32_t MessengerLengthHeaderType;

static const std::string kMessengerHeartBeatMessageType = "heartbeat";
static const std::string kMessengerHeartBeatReplyMessageType =
    "heartbeat_reply";
static const event::Duration kMessengerHeartBeatInterval = 1000 /* ms */;
static const event::Duration kMessengerHeartBeatTimeout = 10000 /* ms */;
static const size_t kMessengerOutboundQueueSize = 32;

class Messenger::Heartbeater {
public:
  Heartbeater(Messenger* messenger)
      : messenger_(messenger), beatTimer_(new event::Timer(0)),
        missedTimer_(new event::Timer(kMessengerHeartBeatTimeout)) {
    beater_.reset(new event::Action(
        {beatTimer_->didFire(), messenger_->outboundQ->canPush()}));

    // Sets up periodic heart beat sending
    beater_->callback = [this]() {
      messenger_->outboundQ->push(
          Message(kMessengerHeartBeatMessageType,
                  {{"start", event::Timer::getTimeInMilliseconds()}}));
      beatTimer_->extend(kMessengerHeartBeatInterval);
    };

    // Sets up missed heartbeat disconnection
    event::Trigger::arm({missedTimer_->didFire()}, [this]() {
      LOG_I("Messenger") << "Disconnected due to missed heartbeats."
                         << std::endl;
      messenger_->disconnect();
    });
  }

  void didReceiveHeartbeat(Message const& message) {
    messenger_->outboundQ->push(
        Message(kMessengerHeartBeatReplyMessageType, message.getBody()));
    missedTimer_->reset(kMessengerHeartBeatTimeout);
  }

private:
  Messenger* messenger_;

  std::unique_ptr<event::Timer> beatTimer_;
  std::unique_ptr<event::Action> beater_;
  std::unique_ptr<event::Timer> missedTimer_;
};

Messenger::Messenger(std::unique_ptr<TCPSocket> socket)
    : outboundQ(new event::FIFO<Message>(kMessengerOutboundQueueSize)),
      socket_(std::move(socket)), bufferUsed_(0),
      didDisconnect_(new event::BaseCondition()),
      statLatency_("Connection", "latency", 0) {}

Messenger::~Messenger() {}

void Messenger::start() {
  sender_.reset(new event::Action({socket_->canWrite(), outboundQ->canPop()}));
  sender_->callback.setMethod<Messenger, &Messenger::doSend>(this);
  receiver_.reset(
      new event::Action({socket_->canRead(), outboundQ->canPush()}));
  receiver_->callback.setMethod<Messenger, &Messenger::doReceive>(this);

  heartbeater_.reset(new Heartbeater(this));
}

void Messenger::disconnect() {
  sender_.reset();
  receiver_.reset();
  heartbeater_.reset();
  socket_.reset();
  didDisconnect_->fire();
}

void Messenger::doReceive() {
  try {
    size_t read = socket_->read(buffer_ + bufferUsed_,
                                kMessengerReceiveBufferSize - bufferUsed_);
    bufferUsed_ += read;
  } catch (SocketClosedException const& ex) {
    LOG_I("Messenger") << "While receiving: " << ex.what() << std::endl;
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
      LOG_I("Messenger") << "Disconnected due to invalid message." << std::endl;
      disconnect();
      return;
    }

    bufferUsed_ -= (totalLen);

    if (message.getType() == kMessengerHeartBeatMessageType) {
      heartbeater_->didReceiveHeartbeat(message);
    } else if (message.getType() == kMessengerHeartBeatReplyMessageType) {
      event::Time start = message.getBody()["start"];
      statLatency_.accumulate((event::Timer::getTimeInMilliseconds() - start) /
                              2);
    } else {
      LOG_V("Messenger") << "Received: " << message.getType() << " - "
                         << message.getBody() << std::endl;

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
    LOG_V("Messenger") << "Sent: " << message.getType() << " = "
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
    LOG_I("Messenger") << "While sending: " << ex.what() << std::endl;
    disconnect();
    return;
  }
}

void Messenger::addEncryptor(crypto::Encryptor* encryptor) {
  encryptors_.emplace_back(encryptor);
}

event::Condition* Messenger::didDisconnect() const {
  return didDisconnect_.get();
}
}
