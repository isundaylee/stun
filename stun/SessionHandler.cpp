#include "stun/SessionHandler.h"

#include <stun/CommandCenter.h>

#include <event/Trigger.h>

namespace stun {

using namespace networking;

SessionHandler::SessionHandler(CommandCenter* center, bool isServer,
                               std::string serverAddr, size_t clientIndex,
                               TCPPipe&& client)
    : center_(center), isServer_(isServer), serverAddr_(serverAddr),
      clientIndex(clientIndex), commandPipe_(new TCPPipe(std::move(client))),
      messenger_(new Messenger(*commandPipe_)) {
  attachHandler();
}

SessionHandler::SessionHandler(SessionHandler&& move)
    : commandPipe_(std::move(move.commandPipe_)),
      dataPipe_(std::move(move.dataPipe_)),
      messenger_(std::move(move.messenger_)), tun_(std::move(move.tun_)),
      primer_(std::move(move.primer_)), sender_(std::move(move.sender_)),
      receiver_(std::move(move.receiver_)),
      myTunnelAddr_(std::move(move.myTunnelAddr_)),
      peerTunnelAddr_(std::move(move.peerTunnelAddr_)),
      clientIndex(std::move(move.clientIndex)),
      center_(std::move(move.center_)) {
  attachHandler();
}

SessionHandler& SessionHandler::operator=(SessionHandler&& move) {
  using std::swap;
  swap(commandPipe_, move.commandPipe_);
  swap(dataPipe_, move.dataPipe_);
  swap(messenger_, move.messenger_);
  swap(tun_, move.tun_);
  swap(primer_, move.primer_);
  swap(sender_, move.sender_);
  swap(receiver_, move.receiver_);
  swap(myTunnelAddr_, move.myTunnelAddr_);
  swap(peerTunnelAddr_, move.peerTunnelAddr_);
  swap(clientIndex, move.clientIndex);
  swap(center_, move.center_);
  attachHandler();
  return *this;
}

void SessionHandler::start() {
  messenger_->start();

  if (!isServer_) {
    assertTrue(
        messenger_->canSend()->value,
        "How can I not be able to send at the very start of a connection?");
    messenger_->send(Message("hello", ""));
  }
}

void SessionHandler::attachHandler() {
  messenger_->handler = [this](Message const& message) {
    return (isServer_ ? handleMessageFromClient(message)
                      : handleMessageFromServer(message));
  };
}

void SessionHandler::createDataTunnel(std::string const& tunnelName,
                                      std::string const& myAddr,
                                      std::string const& peerAddr) {
  // Establish tunnel
  tun_.reset(new Tunnel(TunnelType::TUN));
  tun_->setName(tunnelName);
  tun_->open();
  InterfaceConfig client;
  client.newLink(tun_->getDeviceName());
  client.setLinkAddress(tun_->getDeviceName(), myAddr, peerAddr);

  // Configure sender and receiver
  sender_.reset(new PacketTranslator<TunnelPacket, UDPPacket>(
      tun_->inboundQ.get(), dataPipe_->outboundQ.get()));
  sender_->transform = [](TunnelPacket const& in) {
    UDPPacket out;
    out.fill(in.data, in.size);
    return out;
  };
  sender_->start();

  receiver_.reset(new PacketTranslator<UDPPacket, TunnelPacket>(
      dataPipe_->inboundQ.get(), tun_->outboundQ.get()));
  receiver_->transform = [](UDPPacket const& in) {
    TunnelPacket out;
    out.fill(in.data, in.size);
    return out;
  };
  receiver_->start();
}

Message SessionHandler::handleMessageFromClient(Message const& message) {
  std::string type = message.getType();
  json body = message.getBody();

  if (type == "hello") { // Set up data pipe
    dataPipe_.reset(new UDPPipe());
    dataPipe_->setName("DATA-" + std::to_string(clientIndex));
    dataPipe_->shouldOutputStats = true;
    dataPipe_->open();
    int port = dataPipe_->bind(0);

    // Acquire IP addresses
    myTunnelAddr_ = center_->addrPool->acquire();
    peerTunnelAddr_ = center_->addrPool->acquire();

    // Start listening for the UDP primer packet
    primerAcceptor_.reset(new UDPPrimerAcceptor(*dataPipe_));
    primerAcceptor_->start();
    event::Trigger::arm({messenger_->canSend(), primerAcceptor_->didFinish()},
                        [this]() {
                          createDataTunnel("TUNNEL-" +
                                               std::to_string(clientIndex),
                                           myTunnelAddr_, peerTunnelAddr_);
                          messenger_->send(Message("primed", ""));
                          primerAcceptor_.reset();
                        });

    return Message("config", json{
                                 {"server_ip", myTunnelAddr_},
                                 {"client_ip", peerTunnelAddr_},
                                 {"data_port", port},
                             });
  }  else {
    unreachable("Unrecognized client message type: " + type);
  }
}

Message SessionHandler::handleMessageFromServer(Message const& message) {
  std::string type = message.getType();
  json body = message.getBody();
  std::vector<Message> replies;

  if (type == "config") {
    // Create data pipe
    dataPipe_.reset(new UDPPipe());
    dataPipe_->setName("DATA");
    dataPipe_->shouldOutputStats = true;
    dataPipe_->open();
    int port = dataPipe_->bind(0);
    dataPipe_->connect(serverAddr_, body["data_port"]);

    primer_.reset(new UDPPrimer(*dataPipe_));
    primer_->start();

    createDataTunnel("TUNNEL", body["client_ip"], body["server_ip"]);

    InterfaceConfig config;
    std::vector<networking::SubnetAddress> excluded_subnets = {
        SubnetAddress(commandPipe_->peerAddr, 32)};

    if (common::Configerator::hasKey("excluded_subnets")) {
      std::vector<std::string> configExcludedSubnets =
          common::Configerator::getStringArray("excluded_subnets");
      for (std::string const& exclusion : configExcludedSubnets) {
        excluded_subnets.push_back(SubnetAddress(exclusion));
      }
    }

    std::string originalGateway = config.getRoute(commandPipe_->peerAddr);
    for (networking::SubnetAddress const& exclusion : excluded_subnets) {
      config.newRoute(exclusion, originalGateway);
    }

    if (common::Configerator::hasKey("forward_subnets")) {
      for (auto const& subnet :
           common::Configerator::getStringArray("forward_subnets")) {
        config.newRoute(SubnetAddress(subnet), body["server_ip"]);
      }
    }

    return Message::null();
  } else if (type == "primed") {
    primer_.reset();
    return Message::null();
  } else {
    unreachable("Unrecognized server message type: " + type);
  }
}
}
