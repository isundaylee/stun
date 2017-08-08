#include "stun/Server.h"

#include <event/Trigger.h>
#include <networking/IPTables.h>

namespace stun {

using networking::IPTables;

Server::Server(ServerConfig config) : config_(config) {
  IPTables::clear();
  IPTables::masquerade(config.addressPool);
  addrPool.reset(new IPAddressPool(config.addressPool));

  server_.reset(new TCPServer());
  listener_.reset(new event::Action({server_->canAccept()}));
  listener_->callback.setMethod<Server, &Server::doAccept>(this);
  server_->bind(config.port);
}

void Server::doAccept() {
  TCPSocket client = server_->accept();

  LOG_I("Center") << "Accepted a client from "
                  << client.getPeerAddress().getHost() << std::endl;

  auto sessionConfig =
      ServerSessionConfig{config_.encryption, config_.secret, config_.paddingTo,
                          config_.dataPipeRotationInterval};

  auto handler = std::make_unique<ServerSessionHandler>(
      this, sessionConfig, std::make_unique<TCPSocket>(std::move(client)));

  // Trigger to remove finished clients
  auto handlerPtr = handler.get();
  event::Trigger::arm({handler->didEnd()}, [this, handlerPtr]() {
    auto it = std::find_if(sessionHandlers_.begin(), sessionHandlers_.end(),
                           [handlerPtr](auto const& handler) {
                             return handler.get() == handlerPtr;
                           });

    assertTrue(it != sessionHandlers_.end(),
               "Cannot find the client to remove.");
    sessionHandlers_.erase(it);
  });

  sessionHandlers_.push_back(std::move(handler));
}
}
