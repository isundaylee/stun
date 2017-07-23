#pragma once

#include <TCPPipe.h>
#include <PacketTranslator.h>

#include <vector>

namespace stun {

struct Message : PipePacket {
  Message() :
      PipePacket() {}

  Message(std::string const& type, std::string const& body) :
      PipePacket() {
    appendString(type);
    appendString(body);
  }

  std::string getType() const {
    return std::string(buffer);
  }

  std::string getBody() const {
    int typeLen = strlen(buffer);
    std::string body = std::string(buffer + (typeLen + 1));
    assertTrue(typeLen + body.length() + 2 == size, "Message size not matching");
    return body;
  }

private:
  void appendString(std::string str) {
    memcpy(buffer + size, str.c_str(), str.length());
    buffer[size + str.length()] = '\0';
    size += (str.length() + 1);
  }
};

const size_t kMessengerReceiveBufferSize = 8192;

typedef uint32_t MessengerLengthHeaderType;

class Messenger {
public:
  Messenger(TCPPipe& client) :
      bufferUsed_(0),
      translator_(client.inboundQ, client.outboundQ),
      client_(client) {
    translator_.transform = [this](TCPPacket const& in) {
      assertTrue(bufferUsed_ + in.size <= kMessengerReceiveBufferSize, "Messenger receive buffer overflow.");
      memcpy(buffer_ + bufferUsed_, in.buffer, in.size);
      bufferUsed_ += in.size;

      int messageLen = *((MessengerLengthHeaderType*) buffer_);
      if (bufferUsed_ >= messageLen) {
        // We have got a complete message;
        Message message;
        message.size = messageLen;
        memcpy(message.buffer, buffer_ + sizeof(MessengerLengthHeaderType), messageLen);

        if (bufferUsed_ > sizeof(MessengerLengthHeaderType) + messageLen) {
          memcpy(buffer_, buffer_ + sizeof(MessengerLengthHeaderType),
              bufferUsed_ - (sizeof(MessengerLengthHeaderType) + messageLen));
        }

        bufferUsed_ -= (sizeof(MessengerLengthHeaderType) + messageLen);

        LOG() << client_.name << " received: " << message.getType() << " - " << message.getBody() << std::endl;

        auto responses = handler(message);
        for (auto const& response : responses) {
          send(response);
        }
      }

      // TODO: fake an empty back packet
      TCPPacket emptyPacket;
      return emptyPacket;
    };
  }

  void start() {
    translator_.start();
  }

  void send(Message const& message) {
    // TODO: Think about FIFO overflow later
    // TODO: Think about large messages later

    assertTrue(message.size + sizeof(MessengerLengthHeaderType) <= kPipePacketBufferSize,
        "Message too large");

    TCPPacket packet;
    packet.size = sizeof(MessengerLengthHeaderType) + message.size;
    *((MessengerLengthHeaderType*) packet.buffer) = message.size;
    memcpy(packet.buffer + sizeof(MessengerLengthHeaderType), message.buffer, message.size);

    client_.outboundQ.push(packet);
    LOG() << client_.name << " sent: " << message.getType() << " - " << message.getBody() << std::endl;
  }

  std::function<std::vector<Message> (Message const&)> handler;

private:
  TCPPipe& client_;
  PacketTranslator<TCPPacket, TCPPacket> translator_;
  int bufferUsed_;
  char buffer_[kMessengerReceiveBufferSize];
};

}
