#include "networking/Messenger.h"

#include <event/Trigger.h>

#include <chrono>

namespace networking {

using namespace std::chrono_literals;

static const std::string kMessengerHeartBeatMessageType = "heartbeat";
static const std::string kMessengerHeartBeatReplyMessageType =
    "heartbeat_reply";
static const event::Duration kMessengerHeartBeatInterval = 1s /* ms */;
static const event::Duration kMessengerHeartBeatTimeout = 10s /* ms */;
static const size_t kMessengerOutboundQueueSize = 32;

class Messenger::Heartbeater {
public:
  Heartbeater(Messenger* messenger)
      : messenger_(messenger), beatTimer_(new event::Timer(0s)),
        missedTimer_(new event::Timer(kMessengerHeartBeatTimeout)),
        statRtt_("Connection", "rtt") {
    beater_ = messenger_->loop_.createAction(
        {beatTimer_->didFire(), messenger_->outboundQ->canPush()});

    // Sets up periodic heart beat sending
    beater_->callback = [this]() {
      messenger_->outboundQ->push(Message(
          kMessengerHeartBeatMessageType,
          {{"start", event::Timer::getEpochTimeInMilliseconds().count()}}));
      beatTimer_->extend(kMessengerHeartBeatInterval);
    };

    // Sets up missed heartbeat disconnection
    messenger_->loop_.arm({missedTimer_->didFire()}, [this]() {
      LOG_I("Messenger") << "Disconnected due to missed heartbeats."
                         << std::endl;
      messenger_->disconnect();
    });

    // Sets up heartbeat message handler
    messenger_->addHandler(
        kMessengerHeartBeatMessageType, [this](auto const& message) {
          messenger_->outboundQ->push(
              Message(kMessengerHeartBeatReplyMessageType, message.getBody()));
          missedTimer_->reset(kMessengerHeartBeatTimeout);
          return Message::null();
        });

    // Sets up heartbeat reply message handler
    messenger_->addHandler(
        kMessengerHeartBeatReplyMessageType, [this](auto const& message) {
          auto start = message.getBody()["start"].template get<int>();
          statRtt_.accumulate(
              event::Timer::getEpochTimeInMilliseconds().count() - start);
          return Message::null();
        });
  }

private:
  Messenger* messenger_;

  std::unique_ptr<event::Timer> beatTimer_;
  std::unique_ptr<event::Action> beater_;
  std::unique_ptr<event::Timer> missedTimer_;

  stats::AvgStat statRtt_;
};

class Messenger::Transporter {
private:
  using LengthHeaderType = uint32_t;

public:
  Transporter(Messenger* messenger, std::unique_ptr<TCPSocket> socket)
      : messenger_(messenger), socket_(std::move(socket)), bufferUsed_(0),
        sender_(messenger_->loop_.createAction(
            {socket_->canWrite(), messenger->outboundQ->canPop()})),
        receiver_(messenger_->loop_.createAction(
            {socket_->canRead(), messenger->outboundQ->canPush()})) {
    sender_->callback.setMethod<Transporter, &Transporter::doSend>(this);
    receiver_->callback.setMethod<Transporter, &Transporter::doReceive>(this);
  }

  std::vector<std::unique_ptr<crypto::Encryptor>> encryptors_;

  void doReceive() {
    try {
      size_t read = socket_->read(buffer_ + bufferUsed_,
                                  kMessengerReceiveBufferSize - bufferUsed_);
      bufferUsed_ += read;
    } catch (SocketClosedException const& ex) {
      LOG_I("Messenger") << "While receiving: " << ex.what() << std::endl;
      messenger_->disconnect();
      return;
    }

    // Deliver complete messages
    while (bufferUsed_ >= sizeof(LengthHeaderType)) {
      int messageLen = ntohl(*((LengthHeaderType*)buffer_));
      int totalLen = sizeof(LengthHeaderType) + messageLen;
      if (bufferUsed_ < totalLen) {
        break;
      }

      // We have a complete message
      Message message;
      message.fill(buffer_ + sizeof(LengthHeaderType), messageLen);

      LengthHeaderType payloadLen = messageLen;
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
        LOG_I("Messenger") << "Disconnected due to invalid message."
                           << std::endl;
        messenger_->disconnect();
        return;
      }

      bufferUsed_ -= (totalLen);

      LOG_V("Messenger") << "Received: " << message.getType() << " - "
                         << message.getBody() << std::endl;

      //  Dispatch the incoming message to the correct handler
      auto it = messenger_->handlers_.find(message.getType());
      assertTrue(it != messenger_->handlers_.end(),
                 "Unknown message type " + message.getType());
      auto reply = it->second(message);
      if (reply.size > 0) {
        messenger_->outboundQ->push(std::move(reply));
      }
    }
  }

  void doSend() {
    Message message = messenger_->outboundQ->pop();

    if (message.isDisconnect()) {
      LOG_I("Messenger") << "Disconnected." << std::endl;
      messenger_->disconnect();
      return;
    }

    LOG_V("Messenger") << "Sent: " << message.getType() << " = "
                       << message.getBody() << std::endl;

    LengthHeaderType payloadSize = message.size;
    for (auto const& encryptor : encryptors_) {
      payloadSize =
          encryptor->encrypt(message.data, payloadSize, message.capacity);
    }

    try {
      LengthHeaderType networkPayloadSize = htonl(payloadSize);
      int written =
          socket_->write((Byte*)&networkPayloadSize, sizeof(payloadSize));
      assertTrue(written == sizeof(payloadSize),
                 "Message length header fragmented");
      written = socket_->write(message.data, payloadSize);
      assertTrue(written == payloadSize, "Message content fragmented");
    } catch (SocketClosedException const& ex) {
      LOG_I("Messenger") << "While sending: " << ex.what() << std::endl;
      messenger_->disconnect();
      return;
    }
  }

private:
  Messenger* messenger_;

  std::unique_ptr<TCPSocket> socket_;
  int bufferUsed_;
  Byte buffer_[kMessengerReceiveBufferSize];
  std::unique_ptr<event::Action> sender_;
  std::unique_ptr<event::Action> receiver_;
};

Messenger::Messenger(event::EventLoop& loop, std::unique_ptr<TCPSocket> socket)
    : outboundQ(new event::FIFO<Message>(loop, kMessengerOutboundQueueSize)),
      loop_(loop), transporter_(new Transporter(this, std::move(socket))),
      heartbeater_(new Heartbeater(this)),
      didDisconnect_(loop.createBaseCondition()) {}

Messenger::~Messenger() {}

void Messenger::disconnect() {
  transporter_.reset();
  heartbeater_.reset();
  didDisconnect_->fire();
}

void Messenger::addEncryptor(std::unique_ptr<crypto::Encryptor> encryptor) {
  transporter_->encryptors_.emplace_back(std::move(encryptor));
}

void Messenger::addHandler(std::string messageType,
                           std::function<Message(Message const&)> handler) {
  assertTrue(handlers_.find(messageType) == handlers_.end(),
             "Duplicate handler registered for message type " + messageType);
  handlers_[messageType] = handler;
}

event::Condition* Messenger::didDisconnect() const {
  return didDisconnect_.get();
}
} // namespace networking
