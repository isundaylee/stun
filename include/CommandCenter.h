#pragma once

#include <TCPPipe.h>
#include <UDPPipe.h>
#include <Messenger.h>

#include <string>
#include <vector>
#include <memory>

namespace stun {

class AbstractHandler {
public:
  size_t clientIndex;

  AbstractHandler(size_t clientIndex, TCPPipe&& client) :
      clientIndex(clientIndex),
      commandPipe_(new TCPPipe(std::move(client))),
      messenger_(new Messenger(*commandPipe_)) {
    attachHandler();
  }

  AbstractHandler(AbstractHandler&& move) :
      commandPipe_(std::move(move.commandPipe_)),
      dataPipe_(std::move(move.dataPipe_)),
      messenger_(std::move(move.messenger_)) {
    attachHandler();
  }

  AbstractHandler& operator=(AbstractHandler&& move) {
    using std::swap;
    swap(commandPipe_, move.commandPipe_);
    swap(dataPipe_, move.dataPipe_);
    swap(messenger_, move.messenger_);
    swap(clientIndex, move.clientIndex);
    attachHandler();
    return *this;
  }

  virtual void start() {
    messenger_->start();
  }

protected:
  virtual std::vector<Message> handleMessage(Message const& message) = 0;
  std::unique_ptr<TCPPipe> commandPipe_;
  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<UDPPipe> dataPipe_;

private:
  void attachHandler() {
    messenger_->handler = [this](Message const& message) {
      return handleMessage(message);
    };
  }
};

class ServerHandler: public AbstractHandler {
public:
  ServerHandler(int clientIndex, TCPPipe&& client) :
    AbstractHandler(clientIndex, std::move(client)) {}

  explicit ServerHandler(ServerHandler&& move) :
    AbstractHandler(std::move(move)) {}

  ServerHandler& operator=(ServerHandler&& move) {
    AbstractHandler::operator=(std::move(move));
  }

protected:
  virtual std::vector<Message> handleMessage(Message const& message) override {
    std::string type = message.getType();
    std::vector<Message> replies;

    if (type == "hello") {
      dataPipe_.reset(new UDPPipe());
      dataPipe_->name = "DATA-" + std::to_string(clientIndex);
      dataPipe_->open();
      int port = dataPipe_->bind(0);
      replies.emplace_back("data_port", std::to_string(port));
    } else {
      assertTrue(false, "Unrecognized client message type: " + type);
    }

    return replies;
  }
};

class ClientHandler: public AbstractHandler {
public:
  explicit ClientHandler(TCPPipe&& client) :
    AbstractHandler(0, std::move(client)) {}

  explicit ClientHandler(ClientHandler&& move) :
    AbstractHandler(std::move(move)) {}

  ClientHandler& operator=(ClientHandler&& move) {
    AbstractHandler::operator=(std::move(move));
  }

  virtual void start() override {
    AbstractHandler::start();
    messenger_->send(Message("hello", ""));
  }

protected:
  virtual std::vector<Message> handleMessage(Message const& message) override {
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
  static int numClients;

  TCPPipe commandServer;
  std::vector<ServerHandler> servers;
  std::unique_ptr<ClientHandler> client;
};

}
