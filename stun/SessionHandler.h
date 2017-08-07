#pragma once

#include <stddef.h>

#include <stun/DataPipe.h>
#include <stun/Dispatcher.h>

#include <networking/Messenger.h>
#include <networking/TCPServer.h>
#include <networking/TCPSocket.h>
#include <networking/Tunnel.h>
#include <networking/UDPSocket.h>

namespace stun {

struct SessionConfig {
public:
  std::string secret;
  bool encryption;
  size_t paddingTo;
};

using namespace networking;

enum SessionType { Client, Server };

class Server;

class SessionHandler {
public:
  // Session state
  std::string myTunnelAddr;
  std::string peerTunnelAddr;

  SessionHandler(class Server* server, SessionType type, std::string serverAddr,
                 std::unique_ptr<TCPSocket> commandPipe);

  event::Condition* didEnd() const;

protected:
private:
  SessionHandler(SessionHandler const& copy) = delete;
  SessionHandler& operator=(SessionHandler const& copy) = delete;

  SessionHandler(SessionHandler&& move) = delete;
  SessionHandler& operator=(SessionHandler&& move) = delete;

  class Server* server_;

  // Session settings
  SessionType type_;
  std::string serverAddr_;
  std::string serverIPAddr_;

  // Command connection
  std::unique_ptr<Messenger> messenger_;

  // Data connection
  std::unique_ptr<Dispatcher> dispatcher_;

  event::Duration dataPipeRotateInterval_;
  std::unique_ptr<event::Timer> dataPipeRotationTimer_;
  std::unique_ptr<event::Action> dataPipeRotator_;

  std::unique_ptr<event::BaseCondition> didEnd_;

  void attachHandlers();
  Tunnel createTunnel(std::string const& myAddr, std::string const& peerAddr);
  json createDataPipe();
  void attachServerMessageHandlers();
  void attachClientMessageHandlers();
  void doRotateDataPipe();
};

class ServerHandler {
public:
  ServerHandler();
};
}
