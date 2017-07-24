#pragma once

#include <json/json.hpp>

#include <common/Util.h>
#include <networking/PacketTranslator.h>
#include <networking/TCPPipe.h>

#include <vector>

namespace networking {

using json = nlohmann::json;

struct Message : PipePacket {
  Message() : PipePacket() {}

  static Message null() { return Message(); }

  Message(std::string const& type, json const& body) : PipePacket() {
    json payload = {
        {"type", type}, {"body", body},
    };
    std::string content = payload.dump();
    memcpy(buffer, content.c_str(), content.length());
    size = content.length();
  }

  std::string getType() const {
    return json::parse(std::string(buffer, size))["type"];
  }

  json getBody() const {
    return json::parse(std::string(buffer, size))["body"];
  }
};

const size_t kMessengerReceiveBufferSize = 8192;

class Messenger {
public:
  Messenger(TCPPipe& client);

  void start();
  void send(Message const& message);

  std::function<Message(Message const&)> handler;

private:
  Messenger(Messenger const& copy) = delete;
  Messenger& operator=(Messenger const& copy) = delete;

  Messenger(Messenger&& move) = delete;
  Messenger& operator=(Messenger&& move) = delete;

  TCPPipe& client_;

  event::FIFO<TCPPacket>* inboundQ_;
  event::FIFO<TCPPacket>* outboundQ_;

  std::unique_ptr<event::Action> receiver_;
  int bufferUsed_;
  char buffer_[kMessengerReceiveBufferSize];

  void doReceive();
};
}
