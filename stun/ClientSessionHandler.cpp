#include "stun/ClientSessionHandler.h"

#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

#include <chrono>

namespace stun {

const static size_t kClientSessionHandlerRouteChunkSize = 50;

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

  auto helloBody = json{};
  if (!config_.user.empty()) {
    helloBody["user"] = config_.user;
  }
  messenger_->outboundQ->push(Message("hello", helloBody));

  attachHandlers();
}

event::Condition* ClientSessionHandler::didEnd() const { return didEnd_.get(); }

void ClientSessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  event::Trigger::arm({messenger_->didDisconnect()},
                      [this]() { didEnd_->fire(); });

  messenger_->addHandler("config", [this](auto const& message) {
    auto body = message.getBody();

    dispatcher_.reset(new Dispatcher(createTunnel(
        IPAddress(body["client_tunnel_ip"].template get<std::string>()),
        IPAddress(body["server_tunnel_ip"].template get<std::string>()),
        SubnetAddress(body["server_subnet"].template get<std::string>()))));

    LOG_I("Session") << "Received config from the server." << std::endl;

    return Message("config_done", "");
  });

  messenger_->addHandler("new_data_pipe", [this](auto const& message) {
    auto body = message.getBody();

    UDPSocket udpPipe;
    udpPipe.connect(
        SocketAddress(config_.serverAddr.getHost().toString(), body["port"]));

    DataPipe* dataPipe = new DataPipe(
        std::make_unique<UDPSocket>(std::move(udpPipe)), body["aes_key"],
        body["padding_to_size"], body["compression"], 0s);
    dataPipe->setPrePrimed();

    dispatcher_->addDataPipe(std::unique_ptr<DataPipe>(dataPipe));

    LOG_V("Session") << "Rotated to a new data pipe." << std::endl;

    return Message::null();
  });

  messenger_->addHandler("message", [](auto const& message) {
    LOG_I("Session") << "Message from the server: "
                     << message.getBody().template get<std::string>()
                     << std::endl;
    return Message::null();
  });

  messenger_->addHandler("error", [](auto const& message) {
    LOG_I("Session") << "Session ended with error: "
                     << message.getBody().template get<std::string>()
                     << std::endl;
    return Message::disconnect();
  });
}

/* static */ void
ClientSessionHandler::createRoutes(std::vector<Route> routes) {
  InterfaceConfig config;

  // Adding routes under OSX is slow if we have thousands of routes to add.
  // We need to chunk them, as otherwise, doing it all at the same time would
  // starve our Messenger heartbeats, and cause the connection to fail.
  size_t left = kClientSessionHandlerRouteChunkSize;

  while (!routes.empty()) {
    config.newRoute(routes.back());
    routes.pop_back();
    left--;

    if (left == 0) {
      LOG_V("Session") << "Added " << kClientSessionHandlerRouteChunkSize
                       << " routes." << std::endl;

      // We yield the remaining routes to the next event loop iteration
      event::Trigger::perform([routes = std::move(routes)]() {
        ClientSessionHandler::createRoutes(routes);
      });

      break;
    }
  }
}

Tunnel
ClientSessionHandler::createTunnel(IPAddress const& myTunnelAddr,
                                   IPAddress const& peerTunnelAddr,
                                   SubnetAddress const& serverSubnetAddr) {
  Tunnel tunnel;

  // Configure the new interface
  InterfaceConfig config;
  config.newLink(tunnel.deviceName, kTunnelEthernetMTU);
  config.setLinkAddress(tunnel.deviceName, myTunnelAddr, peerTunnelAddr);

  auto routes = std::vector<Route>{};

  // Create routing rules for subnets NOT to forward
  auto excludedSubnets = config_.subnetsToExclude;
  excludedSubnets.emplace_back(config_.serverAddr.getHost(), 32);
  RouteDestination originalRouteDest =
      config.getRoute(config_.serverAddr.getHost());
  for (auto const& exclusion : excludedSubnets) {
    routes.push_back(Route{exclusion, originalRouteDest});
  }

  // Create routing rules for subnets to forward
  auto forwardSubnets = config_.subnetsToForward;
  forwardSubnets.emplace_back(serverSubnetAddr);
  for (auto const& subnet : forwardSubnets) {
    RouteDestination routeDest(peerTunnelAddr);
    routes.push_back(Route{subnet, routeDest});
  }

  ClientSessionHandler::createRoutes(std::move(routes));

  return tunnel;
}
}
