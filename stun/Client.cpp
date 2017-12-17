#include "stun/Client.h"

#include <event/Action.h>
#include <event/Promise.h>
#include <event/Trigger.h>

namespace stun {

using namespace std::chrono_literals;

const static event::Duration kReconnectDelayInterval = 5s;
const static size_t kClientRouteChunkSize = 1;

#if TARGET_IOS
Client::Client(ClientConfig config,
               ClientSessionHandler::TunnelFactory tunnelFactory)
    : config_(config), tunnelFactory_(tunnelFactory) {
  connect();
}
#else
Client::Client(ClientConfig config) : config_(config) {
  tunnelFactory_ = [this](ClientTunnelConfig config) {
    auto promise = std::make_shared<event::Promise<std::unique_ptr<Tunnel>>>();
    promise->fulfill(createTunnel(config));
    return promise;
  };

  connect();
}
#endif

Client::~Client() = default;

/* static */ void Client::createRoutes(std::vector<Route> routes) {
  // Adding routes under OSX is slow if we have thousands of routes to add.
  // We need to chunk them, as otherwise, doing it all at the same time would
  // starve our Messenger heartbeats, and cause the connection to fail.
  size_t left = kClientRouteChunkSize;

  while (!routes.empty()) {
    InterfaceConfig::newRoute(routes.back());
    routes.pop_back();
    left--;

    if (left == 0) {
      LOG_V("Session") << "Added " << kClientRouteChunkSize << " routes."
                       << std::endl;

      // We yield the remaining routes to the next event loop iteration
      event::Trigger::perform([routes = std::move(routes)]() {
        Client::createRoutes(routes);
      });

      break;
    }
  }
}

std::unique_ptr<Tunnel> Client::createTunnel(ClientTunnelConfig config) {
  auto tunnel = std::make_unique<Tunnel>();

  // Configure the new interface
  InterfaceConfig::newLink(tunnel->deviceName, config.mtu);
  InterfaceConfig::setLinkAddress(tunnel->deviceName, config.myTunnelAddr,
                                  config.peerTunnelAddr);

  auto routes = std::vector<Route>{};

  // Create routing rules for subnets NOT to forward
  auto excludedSubnets = config_.subnetsToExclude;

  if (config_.serverAddr.type == IPv4) {
    RouteDestination originalRouteDest =
        InterfaceConfig::getRoute(config_.serverAddr.getHost());
    if (!originalRouteDest.gatewayAddr.empty()) {
      // If original route doesn't have a gateway, it *likely* means that the
      // matching route is an ARP route. Thus we don't add the route explicitly
      // again. FIXME: Revisit this.
      excludedSubnets.emplace_back(config_.serverAddr.getHost(), 32);
    } else {
      LOG_I("Client")
          << "Seems like server is on the same network as us. Won't "
             "create explicit exclusion route to server. "
          << std::endl;
    }
    for (auto const& exclusion : excludedSubnets) {
      routes.push_back(Route{exclusion, originalRouteDest});
    }
  } else {
    LOG_I("Client") << "Seems like we're on an 6-to-4 network. Won't create "
                       "explicit exclusion route to server. "
                    << std::endl;
  }

  // Create routing rules for subnets to forward
  auto forwardSubnets = config_.subnetsToForward;
  forwardSubnets.emplace_back(config.serverSubnetAddr);
  for (auto const& subnet : forwardSubnets) {
    RouteDestination routeDest(config.peerTunnelAddr);
    routes.push_back(Route{subnet, routeDest});
  }

  Client::createRoutes(std::move(routes));

  return tunnel;
}

void Client::connect() {
  auto socket = TCPSocket{config_.serverAddr.type};
  socket.connect(config_.serverAddr);

  handler_.reset(new ClientSessionHandler(
      config_, std::make_unique<TCPSocket>(std::move(socket)), tunnelFactory_));
  reconnector_.reset(new event::Action({handler_->didEnd()}));
  reconnector_->callback.setMethod<Client, &Client::doReconnect>(this);
}

void Client::doReconnect() {
  handler_.reset();
  reconnector_.reset();

  LOG_I("Client") << "Will reconnect in "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         kReconnectDelayInterval)
                         .count()
                  << " ms." << std::endl;

  event::Trigger::performIn(kReconnectDelayInterval, [this]() {
    LOG_I("Client") << "Reconnecting..." << std::endl;
    connect();
  });
}
}
