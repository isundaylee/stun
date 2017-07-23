#pragma once

#include <TCPPipe.h>
#include <UDPPipe.h>
#include <Tunnel.h>
#include <NetlinkClient.h>
#include <Messenger.h>
#include <IPAddressPool.h>

#include <string>
#include <vector>
#include <memory>

namespace stun {

class ServerHandler;
class ClientHandler;

class CommandCenter {
public:
  CommandCenter() :
      addrPool(new IPAddressPool("10.100.0.0", 16)) {}

  void serve(int port);
  void connect(std::string const& host, int port);

  std::unique_ptr<IPAddressPool> addrPool;

private:
  static int numClients;

  void handleAccept(TCPPipe&& client);

  TCPPipe commandServer;
  std::vector<ServerHandler> servers;
  std::unique_ptr<ClientHandler> client;
};

class AbstractHandler {
public:
  size_t clientIndex;

  AbstractHandler(CommandCenter* center, size_t clientIndex, TCPPipe&& client) :
      center_(center),
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
    swap(tun_, move.tun_);
    swap(sender_, move.sender_);
    swap(receiver_, move.receiver_);
    swap(clientIndex, move.clientIndex);
    swap(center_, move.center_);
    attachHandler();
    return *this;
  }

  virtual void start() {
    messenger_->start();
  }

protected:
  virtual std::vector<Message> handleMessage(Message const& message) = 0;
  CommandCenter* center_;
  std::unique_ptr<TCPPipe> commandPipe_;
  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<UDPPipe> dataPipe_;
  std::unique_ptr<Tunnel> tun_;
  std::unique_ptr<PacketTranslator<TunnelPacket, UDPPacket>> sender_;
  std::unique_ptr<PacketTranslator<UDPPacket, TunnelPacket>> receiver_;

private:
  void attachHandler() {
    messenger_->handler = [this](Message const& message) {
      return handleMessage(message);
    };
  }
};

class ServerHandler: public AbstractHandler {
public:
  ServerHandler(CommandCenter* center, int clientIndex, TCPPipe&& client) :
    AbstractHandler(center, clientIndex, std::move(client)) {}

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
      // Set up data pipe
      dataPipe_.reset(new UDPPipe());
      dataPipe_->name = "DATA-" + std::to_string(clientIndex);
      dataPipe_->shouldOutputStats = true;
      dataPipe_->open();
      int port = dataPipe_->bind(0);
      replies.emplace_back("data_port", std::to_string(port));

      // Acquire IP addresses
      std::string const& myAddr = center_->addrPool->acquire();
      std::string const& peerAddr = center_->addrPool->acquire();
      LOG() << "Acquired IP address: mine = " << myAddr << ", peer = " << peerAddr << std::endl;
      replies.emplace_back("server_ip", myAddr);
      replies.emplace_back("client_ip", peerAddr);

      // Establish tunnel
      tun_.reset(new Tunnel(TunnelType::TUN));
      tun_->open();
      NetlinkClient client;
      client.newLink(tun_->getDeviceName());
      client.setLinkAddress(tun_->getDeviceName(), myAddr, peerAddr);

      // Configure sender and receiver
      sender_.reset(new PacketTranslator<TunnelPacket, UDPPacket>(tun_->inboundQ, dataPipe_->outboundQ));
      sender_->transform = [](TunnelPacket const& in) {
        UDPPacket out;
        out.size = in.size;
        memcpy(out.buffer, in.buffer, in.size);
        return out;
      };
      sender_->start();

      receiver_.reset(new PacketTranslator<UDPPacket, TunnelPacket>(dataPipe_->inboundQ, tun_->outboundQ));
      receiver_->transform = [](UDPPacket const& in) {
        TunnelPacket out;
        out.size = in.size;
        memcpy(out.buffer, in.buffer, in.size);
        return out;
      };
      receiver_->start();
    } else {
      assertTrue(false, "Unrecognized client message type: " + type);
    }

    return replies;
  }
};

class ClientHandler: public AbstractHandler {
public:
  explicit ClientHandler(CommandCenter* center, std::string const& host, TCPPipe&& client) :
    AbstractHandler(center, 0, std::move(client)),
    host_(host) {}

  explicit ClientHandler(ClientHandler&& move) :
    AbstractHandler(std::move(move)),
    host_(std::move(move.host_)) {}

  ClientHandler& operator=(ClientHandler&& move) {
    AbstractHandler::operator=(std::move(move));
    std::swap(host_, move.host_);
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
      dataPort_ = std::stoi(body);
    } else if (type == "server_ip") {
      serverIP_ = body;
    } else if (type == "client_ip") {
      clientIP_ = body;
    } else {
      assertTrue(false, "Unrecognized server message type: " + type);
    }

    if (dataPort_ != 0 && !serverIP_.empty() && !clientIP_.empty()) {
      // Create data pipe
      dataPipe_.reset(new UDPPipe());
      dataPipe_->name = "DATA-" + std::to_string(clientIndex);
      dataPipe_->shouldOutputStats = true;
      dataPipe_->open();
      dataPipe_->connect(host_, dataPort_);

      // Create the tunnel
      tun_.reset(new Tunnel(TunnelType::TUN));
      tun_->open();
      NetlinkClient client;
      client.newLink(tun_->getDeviceName());
      client.setLinkAddress(tun_->getDeviceName(), clientIP_, serverIP_);

      // Configure sender and receiver
      sender_.reset(new PacketTranslator<TunnelPacket, UDPPacket>(tun_->inboundQ, dataPipe_->outboundQ));
      sender_->transform = [](TunnelPacket const& in) {
        UDPPacket out;
        out.size = in.size;
        memcpy(out.buffer, in.buffer, in.size);
        return out;
      };
      sender_->start();

      receiver_.reset(new PacketTranslator<UDPPacket, TunnelPacket>(dataPipe_->inboundQ, tun_->outboundQ));
      receiver_->transform = [](UDPPacket const& in) {
        TunnelPacket out;
        out.size = in.size;
        memcpy(out.buffer, in.buffer, in.size);
        return out;
      };
      receiver_->start();
    }

    return replies;
  }

private:
  std::string host_;

  int dataPort_ = 0;
  std::string serverIP_ = "";
  std::string clientIP_ = "";
};

}
