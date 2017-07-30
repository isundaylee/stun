#pragma once

#include <stddef.h>

#include <stun/DataPipe.h>
#include <stun/Dispatcher.h>

#include <networking/Messenger.h>
#include <networking/TCPPipe.h>
#include <networking/Tunnel.h>
#include <networking/UDPPipe.h>
#include <networking/UDPPrimer.h>

namespace stun {

using namespace networking;

class CommandCenter;

class SessionHandler {
public:
  // Session state
  size_t clientIndex;
  std::string myTunnelAddr;
  std::string peerTunnelAddr;

  SessionHandler(CommandCenter* center, bool isServer, std::string serverAddr,
                 size_t clientIndex, TCPPipe&& client);

  SessionHandler(SessionHandler&& move);

  SessionHandler& operator=(SessionHandler&& move);

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

  void attachHandlers();
  Tunnel createTunnel(std::string const& tunnelName, std::string const& myAddr,
                      std::string const& peerAddr);
  Message handleMessageFromClient(Message const& message);
  Message handleMessageFromServer(Message const& message);
};

class ServerHandler {
public:
  ServerHandler();
};
}
