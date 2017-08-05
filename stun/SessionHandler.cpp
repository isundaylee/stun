#include "stun/SessionHandler.h"

#include <common/Configerator.h>
#include <crypto/AESEncryptor.h>
#include <crypto/Padder.h>
#include <event/Trigger.h>
#include <stun/CommandCenter.h>
#include <stun/Dispatcher.h>

namespace stun {

static const event::Duration kSessionHandlerRotationGracePeriod = 5000 /* ms */;

using namespace networking;

SessionHandler::SessionHandler(CommandCenter* center, bool isServer,
                               std::string serverAddr, size_t clientIndex,
                               std::unique_ptr<TCPSocket> commandPipe)
    : clientIndex(clientIndex), dataPipeSeq(0), center_(center),
      isServer_(isServer), serverAddr_(serverAddr),
      messenger_(new Messenger(std::move(commandPipe))),
      didEnd_(new event::BaseCondition()) {
  if (common::Configerator::hasKey("secret")) {
    messenger_->addEncryptor(new crypto::AESEncryptor(
        crypto::AESKey(common::Configerator::getString("secret"))));
  }

  if (isServer) {
    serverIPAddr_ = SocketAddress(serverAddr_).getHost();
  }

  attachHandlers();
}

void SessionHandler::start() {
  messenger_->start();

  if (!isServer_) {
    assertTrue(
        messenger_->outboundQ->canPush()->eval(),
        "How can I not be able to send at the very start of a connection?");
    messenger_->outboundQ->push(Message("hello", ""));
  }
}

event::Condition* SessionHandler::didEnd() const { return didEnd_.get(); }

void SessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  event::Trigger::arm({messenger_->didDisconnect()},
                      [this]() { didEnd_->fire(); });

  messenger_->handler = [this](Message const& message) {
    return (isServer_ ? handleMessageFromClient(message)
                      : handleMessageFromServer(message));
  };
}

Tunnel SessionHandler::createTunnel(std::string const& tunnelName,
                                    std::string const& myTunnelAddr,
                                    std::string const& peerTunnelAddr) {
  Tunnel tunnel;

  // Configure the new interface
  InterfaceConfig config;
  config.newLink(tunnel.deviceName, kTunnelEthernetMTU);
  config.setLinkAddress(tunnel.deviceName, myTunnelAddr, peerTunnelAddr);

  if (!isServer_) {
    // Configure iptables to route traffic into (or not into) the new tunnel
    std::vector<networking::SubnetAddress> excluded_subnets = {
        SubnetAddress(serverIPAddr_, 32)};

    // Create routing rules for subnets NOT to forward
    if (common::Configerator::hasKey("excluded_subnets")) {
      std::vector<std::string> configExcludedSubnets =
          common::Configerator::getStringArray("excluded_subnets");
      for (std::string const& exclusion : configExcludedSubnets) {
        excluded_subnets.push_back(SubnetAddress(exclusion));
      }
    }

    networking::RouteDestination originalRouteDest =
        config.getRoute(serverIPAddr_);
    for (networking::SubnetAddress const& exclusion : excluded_subnets) {
      config.newRoute(exclusion, originalRouteDest);
    }

    // Create routing rules for subnets to forward
    if (common::Configerator::hasKey("forward_subnets")) {
      std::string serverIP = peerTunnelAddr;
      for (auto const& subnet :
           common::Configerator::getStringArray("forward_subnets")) {
        networking::RouteDestination routeDest(serverIP);
        config.newRoute(SubnetAddress(subnet), routeDest);
      }
    }
  }

  return tunnel;
}

json SessionHandler::createDataPipe() {
  dataPipeSeq++;
  UDPSocket udpPipe;
  int port = udpPipe.bind(0);

  // Prepare encryption config
  std::string aesKey = crypto::AESKey::randomStringKey();
  size_t paddingMinSize = 0;
  if (common::Configerator::hasKey("padding_to")) {
    paddingMinSize = common::Configerator::getInt("padding_to");
  }

  size_t ttl = 0;
  if (common::Configerator::hasKey("data_pipe_rotate_interval")) {
    ttl = 1000 * common::Configerator::getInt("data_pipe_rotate_interval") +
          kSessionHandlerRotationGracePeriod;
  }

  DataPipe* dataPipe =
      new DataPipe(std::make_unique<UDPSocket>(std::move(udpPipe)), aesKey,
                   paddingMinSize, ttl);
  dataPipe->start();
  dispatcher_->addDataPipe(std::unique_ptr<DataPipe>{dataPipe});

  return json{
      {"port", port}, {"aes_key", aesKey}, {"padding_to_size", paddingMinSize}};
}

void SessionHandler::doRotateDataPipe() {
  messenger_->outboundQ->push(Message("new_data_pipe", createDataPipe()));
  dataPipeRotationTimer_->extend(dataPipeRotateInterval_);
}

Message SessionHandler::handleMessageFromClient(Message const& message) {
  std::string type = message.getType();
  json body = message.getBody();

  if (type == "hello") {
    // Acquire IP addresses
    myTunnelAddr = center_->addrPool->acquire();
    peerTunnelAddr = center_->addrPool->acquire();

    // Set up the data tunnel. Data pipes will be set up in a later stage.
    dispatcher_.reset(
        new Dispatcher(createTunnel("Tunnel " + std::to_string(clientIndex),
                                    myTunnelAddr, peerTunnelAddr)));
    dispatcher_->start();

    // Set up data pipe rotation if it is configured in the server config.
    if (common::Configerator::hasKey("data_pipe_rotate_interval")) {
      dataPipeRotateInterval_ =
          1000 * common::Configerator::getInt("data_pipe_rotate_interval");
      dataPipeRotationTimer_.reset(new event::Timer(dataPipeRotateInterval_));
      dataPipeRotator_.reset(
          new event::Action({dataPipeRotationTimer_->didFire(),
                             messenger_->outboundQ->canPush()}));
      dataPipeRotator_->callback
          .setMethod<SessionHandler, &SessionHandler::doRotateDataPipe>(this);
    }

    return Message("config", json{{"server_tunnel_ip", myTunnelAddr},
                                  {"client_tunnel_ip", peerTunnelAddr}});
  } else if (type == "config_done") {
    return Message("new_data_pipe", createDataPipe());
  } else {
    unreachable("Unrecognized client message type: " + type);
  }
}

Message SessionHandler::handleMessageFromServer(Message const& message) {
  std::string type = message.getType();
  json body = message.getBody();
  std::vector<Message> replies;

  if (type == "config") {
    dispatcher_.reset(new Dispatcher(createTunnel(
        "Tunnel", body["client_tunnel_ip"], body["server_tunnel_ip"])));
    dispatcher_->start();

    return Message("config_done", "");
  } else if (type == "new_data_pipe") {
    UDPSocket udpPipe;

    dataPipeSeq++;
    udpPipe.connect(SocketAddress(serverIPAddr_, body["port"]));

    DataPipe* dataPipe =
        new DataPipe(std::make_unique<UDPSocket>(std::move(udpPipe)),
                     body["aes_key"], body["padding_to_size"], 0);
    dataPipe->setPrePrimed();
    dataPipe->start();
    dispatcher_->addDataPipe(std::unique_ptr<DataPipe>(dataPipe));

    return Message::null();
  } else {
    unreachable("Unrecognized server message type: " + type);
  }
}
}
