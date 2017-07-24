#pragma once

#include <stddef.h>

#include <networking/Messenger.h>
#include <networking/TCPPipe.h>
#include <networking/Tunnel.h>
#include <networking/UDPPipe.h>

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
  bool isServer_;
  std::string serverAddr_;
  std::unique_ptr<TCPPipe> commandPipe_;
  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<UDPPipe> dataPipe_;
  std::unique_ptr<Tunnel> tun_;
  std::unique_ptr<PacketTranslator<TunnelPacket, UDPPacket>> sender_;
  std::unique_ptr<PacketTranslator<UDPPacket, TunnelPacket>> receiver_;

  // TODO: Rid of this temporary storage
  std::string serverIP_;
  std::string clientIP_;
  int dataPort_;

  void attachHandler();
  void createDataTunnel(std::string const& myAddr, std::string const& peerAddr);
  std::vector<Message> handleMessageFromClient(Message const& message);
  std::vector<Message> handleMessageFromServer(Message const& message);
};

class ServerHandler {
public:
  ServerHandler();
};
}
