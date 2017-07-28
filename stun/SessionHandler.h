#pragma once

#include <stddef.h>

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
  size_t clientIndex;

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
  std::unique_ptr<UDPPipe> dataPipe_;
  std::unique_ptr<Tunnel> tun_;
  std::unique_ptr<UDPPrimer> primer_;
  std::unique_ptr<UDPPrimerAcceptor> primerAcceptor_;
  std::unique_ptr<PacketTranslator<TunnelPacket, UDPPacket>> sender_;
  std::unique_ptr<PacketTranslator<UDPPacket, TunnelPacket>> receiver_;

  std::string myTunnelAddr_;
  std::string peerTunnelAddr_;

  void attachHandler();
  void createDataTunnel(std::string const& tunnelName,
                        std::string const& myAddr, std::string const& peerAddr);
  Message handleMessageFromClient(Message const& message);
  Message handleMessageFromServer(Message const& message);
};

class ServerHandler {
public:
  ServerHandler();
};
}
