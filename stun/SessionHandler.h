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

using namespace networking;

class CommandCenter;

class SessionHandler {
public:
  // Session state
  size_t clientIndex;
  std::string myTunnelAddr;
  std::string peerTunnelAddr;
  size_t dataPipeSeq;

  SessionHandler(CommandCenter* center, bool isServer, std::string serverAddr,
                 size_t clientIndex, std::unique_ptr<TCPSocket> commandPipe);

  void start();
  event::Condition* didEnd() const;

protected:
private:
  SessionHandler(SessionHandler const& copy) = delete;
  SessionHandler& operator=(SessionHandler const& copy) = delete;

  SessionHandler(SessionHandler&& move) = delete;
  SessionHandler& operator=(SessionHandler&& move) = delete;

  CommandCenter* center_;

  // Session settings
  bool isServer_;
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
  Tunnel createTunnel(std::string const& tunnelName, std::string const& myAddr,
                      std::string const& peerAddr);
  json createDataPipe();
  Message handleMessageFromClient(Message const& message);
  Message handleMessageFromServer(Message const& message);
  void doRotateDataPipe();
};

class ServerHandler {
public:
  ServerHandler();
};
}
