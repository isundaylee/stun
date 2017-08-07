#pragma once

#include <json/json.hpp>

#include <stun/Dispatcher.h>

#include <networking/IPAddressPool.h>
#include <networking/Messenger.h>
#include <networking/TCPSocket.h>

namespace stun {

using json = nlohmann::json;

using networking::TCPSocket;
using networking::Messenger;
using networking::SubnetAddress;

struct ServerConfig {
public:
  int port;
  SubnetAddress addressPool;
  bool encryption;
  std::string secret;
  size_t paddingTo;
  event::Duration dataPipeRotationInterval;
};

class Server;

class ServerSessionHandler {
public:
  ServerSessionHandler(Server* server, ServerConfig config,
                       std::unique_ptr<TCPSocket> commandPipe);

  event::Condition* didEnd() const;

private:
  Server* server_;
  ServerConfig config_;

  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<Dispatcher> dispatcher_;

  std::unique_ptr<event::Timer> dataPipeRotationTimer_;
  std::unique_ptr<event::Action> dataPipeRotator_;

  std::unique_ptr<event::BaseCondition> didEnd_;

  void attachHandlers();
  json createDataPipe();
  void doRotateDataPipe();
};
}
