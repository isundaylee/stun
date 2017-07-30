#pragma once

#include <json/json.hpp>

#include <networking/PacketTranslator.h>
#include <networking/TCPPipe.h>

#include <common/Util.h>
#include <crypto/Encryptor.h>
#include <event/Timer.h>

#include <vector>

namespace networking {

using json = nlohmann::json;

const size_t kMessageSize = 2048;

struct Message : Packet<kMessageSize> {
  Message() : Packet() {}

  static Message null() { return Message(); }

  Message(std::string const& type, json const& body) : Packet() {
    json payload = {
        {"type", type}, {"body", body},
    };
    std::string content = payload.dump();
    memcpy(data, content.c_str(), content.length());
    size = content.length();
  }

  std::string getType() const {
    return json::parse(
        std::string(reinterpret_cast<char*>(data), size))["type"];
  }

  json getBody() const {
    return json::parse(
        std::string(reinterpret_cast<char*>(data), size))["body"];
  }

  bool isValid() const {
    try {
      auto type = getType();
      auto body = getBody();
    } catch (const json::exception&) {
      return false;
    }
    return true;
  }
};

const size_t kMessengerReceiveBufferSize = 8192;

class Messenger {
public:
  Messenger(TCPPipe& client);

  void start();

  event::Condition* canSend();
  void send(Message const& message);
  void addEncryptor(crypto::Encryptor* encryptor);

  event::Condition* didReceiveInvalidMessage() const;
  event::Condition* didMissHeartbeat() const;

  std::function<Message(Message const&)> handler;

private:
  Messenger(Messenger const& copy) = delete;
  Messenger& operator=(Messenger const& copy) = delete;

  Messenger(Messenger&& move) = delete;
  Messenger& operator=(Messenger&& move) = delete;

  TCPPipe& client_;

  event::FIFO<TCPPacket>* inboundQ_;
  event::FIFO<TCPPacket>* outboundQ_;

  std::vector<std::unique_ptr<crypto::Encryptor>> encryptors_;

  int bufferUsed_;
  Byte buffer_[kMessengerReceiveBufferSize];
  std::unique_ptr<event::Action> receiver_;
  std::unique_ptr<event::Timer> heartbeatTimer_;
  std::unique_ptr<event::Timer> heartbeatMissedTimer_;
  std::unique_ptr<event::Action> heartbeatSender_;

  std::unique_ptr<event::Condition> didReceiveInvalidMessage_;
  std::unique_ptr<event::Condition> didMissHeartbeat_;

  void doReceive();
};
}
