#include "stun/ClientSessionHandler.h"

#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

#include <chrono>

namespace stun {

using namespace std::chrono_literals;

ClientSessionHandler::ClientSessionHandler(
    ClientConfig config, std::unique_ptr<TCPSocket> commandPipe)
    : config_(config), messenger_(new Messenger(std::move(commandPipe))),
      didEnd_(new event::BaseCondition()) {

  if (!config_.secret.empty()) {
    messenger_->addEncryptor(
        std::make_unique<crypto::AESEncryptor>(crypto::AESKey(config_.secret)));
  }

  assertTrue(
      messenger_->outboundQ->canPush()->eval(),
      "How can I not be able to send at the very start of a connection?");
  messenger_->outboundQ->push(Message("hello", ""));

  attachHandlers();
}

event::Condition* ClientSessionHandler::didEnd() const { return didEnd_.get(); }

void ClientSessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  event::Trigger::arm({messenger_->didDisconnect()},
                      [this]() { didEnd_->fire(); });

  messenger_->addHandler("config", [this](auto const& message) {
    auto body = message.getBody();

    dispatcher_.reset(new Dispatcher(
        createTunnel(body["client_tunnel_ip"], body["server_tunnel_ip"])));

    LOG_I("Session") << "Received config from the server." << std::endl;

    return Message("config_done", "");
  });

  messenger_->addHandler("new_data_pipe", [this](auto const& message) {
    auto body = message.getBody();

    UDPSocket udpPipe;
    udpPipe.connect(SocketAddress(config_.serverAddr.getHost(), body["port"]));

    DataPipe* dataPipe =
        new DataPipe(std::make_unique<UDPSocket>(std::move(udpPipe)),
                     body["aes_key"], body["padding_to_size"], 0s);
    dataPipe->setPrePrimed();

    dispatcher_->addDataPipe(std::unique_ptr<DataPipe>(dataPipe));

    LOG_I("Session") << "Rotated to a new data pipe." << std::endl;

    return Message::null();
  });

  messenger_->addHandler("message", [](auto const& message) {
    LOG_I("Session") << "Message from the server: "
                     << message.getBody().template get<std::string>()
                     << std::endl;
    return Message::null();
  });
}

Tunnel ClientSessionHandler::createTunnel(std::string const& myTunnelAddr,
                                          std::string const& peerTunnelAddr) {
  Tunnel tunnel;

  // Configure the new interface
  InterfaceConfig config;
  config.newLink(tunnel.deviceName, kTunnelEthernetMTU);
  config.setLinkAddress(tunnel.deviceName, myTunnelAddr, peerTunnelAddr);

  // Create routing rules for subnets NOT to forward
  auto excluded_subnets = config_.subnetsToExclude;
  excluded_subnets.emplace_back(config_.serverAddr.getHost(), 32);
  networking::RouteDestination originalRouteDest =
      config.getRoute(config_.serverAddr.getHost());
  for (networking::SubnetAddress const& exclusion : excluded_subnets) {
    config.newRoute(exclusion, originalRouteDest);
  }

  // Create routing rules for subnets to forward
  for (auto const& subnet : config_.subnetsToForward) {
    networking::RouteDestination routeDest(peerTunnelAddr);
    config.newRoute(SubnetAddress(subnet), routeDest);
  }

  return tunnel;
}
}
