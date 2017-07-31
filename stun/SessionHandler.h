#pragma once

#include <stddef.h>

#include <stun/DataPipe.h>
#include <stun/Dispatcher.h>

#include <networking/Messenger.h>
#include <networking/TCPPipe.h>
#include <networking/Tunnel.h>
#include <networking/UDPPipe.h>

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
                 size_t clientIndex, TCPPipe&& client);

  void start();

protected:
private:
  CommandCenter* center_;

  // Session settings
  bool isServer_;
  std::string serverAddr_;

  // Command connection
  std::unique_ptr<TCPPipe> commandPipe_;
  std::unique_ptr<Messenger> messenger_;

  // Data connection
  std::unique_ptr<Dispatcher> dispatcher_;

  event::Duration dataPipeRotateInterval_;
  std::unique_ptr<event::Timer> dataPipeRotationTimer_;
  std::unique_ptr<event::Action> dataPipeRotator_;

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
