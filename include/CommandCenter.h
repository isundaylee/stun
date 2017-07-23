#pragma once

#include <TCPPipe.h>
#include <UDPPipe.h>
#include <Messenger.h>

#include <string>
#include <vector>
#include <memory>

namespace stun {

class ServerHandler {
public:
  ServerHandler(size_t clientIndex, TCPPipe&& client) :
      clientIndex_(clientIndex),
      commandPipe_(new TCPPipe(std::move(client))),
      messenger_(new Messenger(*commandPipe_)) {
    attachHandler();
  }

  ServerHandler(ServerHandler&& move) :
      commandPipe_(std::move(move.commandPipe_)),
      messenger_(std::move(move.messenger_)) {
    attachHandler();
  }

  void start() {
    messenger_->start();
  }

private:
  size_t clientIndex_;
  std::unique_ptr<TCPPipe> commandPipe_;
  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<UDPPipe> dataPipe_;

  void attachHandler() {
    messenger_->handler = [this](Message const& message) {
      return handleMessage(message);
    };
  }

  std::vector<Message> handleMessage(Message const& message) {
    std::string type = message.getType();
    std::vector<Message> replies;

    if (type == "hello") {
      dataPipe_.reset(new UDPPipe());
      dataPipe_->name = "DATA-" + std::to_string(clientIndex_);
      dataPipe_->open();
      int port = dataPipe_->bind(0);
      replies.emplace_back("data_port", std::to_string(port));
    } else {
      assertTrue(false, "Unrecognized client message type: " + type);
    }

    return replies;
  }
};

class ClientHandler {
public:
  ClientHandler(TCPPipe&& server) :
      commandPipe_(new TCPPipe(std::move(server))),
      messenger_(new Messenger(*commandPipe_)) {
    attachHandler();
  }

  ClientHandler(ClientHandler&& move) :
      commandPipe_(std::move(move.commandPipe_)),
      messenger_(std::move(move.messenger_)) {
    attachHandler();
  }

  void start() {
    messenger_->start();
    messenger_->send(Message("hello", ""));
  }

private:
  std::unique_ptr<TCPPipe> commandPipe_;
  std::unique_ptr<Messenger> messenger_;

  void attachHandler() {
    messenger_->handler = [this](Message const& message) {
      return handleMessage(message);
    };
  }

  std::vector<Message> handleMessage(Message const& message) {
    std::string type = message.getType();
    std::string body = message.getBody();
    std::vector<Message> replies;

    if (type == "data_port") {
      int port = std::stoi(body);
    } else {
      assertTrue(false, "Unrecognized server message type: " + type);
    }

    return replies;
  }
};

class CommandCenter {
public:
  void serve(int port);
  void connect(std::string const& host, int port);

private:
  TCPPipe commandServer;
  std::vector<ServerHandler> servers;
  std::unique_ptr<ClientHandler> client;
};

}
