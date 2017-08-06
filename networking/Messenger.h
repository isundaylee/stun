#pragma once

#include <json/json.hpp>

#include <networking/Packet.h>
#include <networking/TCPSocket.h>

#include <common/Util.h>
#include <crypto/Encryptor.h>
#include <event/FIFO.h>
#include <event/Timer.h>
#include <stats/AvgStat.h>

#include <vector>

namespace networking {

using json = nlohmann::json;

const size_t kMessageSize = 2048;

struct Message : public Packet {
  Message() : Packet(kMessageSize) {}

  static Message null() { return Message(); }

  Message(std::string const& type, json const& body) : Packet(kMessageSize) {
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
  Messenger(std::unique_ptr<TCPSocket> socket);
  ~Messenger();

  std::unique_ptr<event::FIFO<Message>> outboundQ;

  void addEncryptor(crypto::Encryptor* encryptor);
  void addHandler(std::string messageType,
                  std::function<Message(Message const&)> handler);
  event::Condition* didDisconnect() const;

private:
  Messenger(Messenger const& copy) = delete;
  Messenger& operator=(Messenger const& copy) = delete;

  Messenger(Messenger&& move) = delete;
  Messenger& operator=(Messenger&& move) = delete;

  class Heartbeater;
  class Transporter;

  std::map<std::string, std::function<Message(Message const&)>> handlers_;
  std::unique_ptr<Transporter> transporter_;
  std::unique_ptr<Heartbeater> heartbeater_;

  std::unique_ptr<event::BaseCondition> didDisconnect_;

  void disconnect();
};
}
