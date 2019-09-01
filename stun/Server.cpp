#include "stun/Server.h"

#include <event/Trigger.h>
#include <networking/IPTables.h>

namespace stun {

using networking::IPTables;

Server::Server(event::EventLoop& loop, ServerConfig config)
    : loop_(loop), config_(config) {
  IPTables::clear(config.configID);
  IPTables::masquerade(config.addressPool, config.configID);
  addrPool.reset(new IPAddressPool(config.addressPool));

  for (auto const& entry : config_.staticHosts) {
    addrPool->reserve(entry.second);
  }

  server_.reset(new TCPServer(loop, networking::NetworkType::IPv4));
  listener_ = loop_.createAction({server_->canAccept()});
  listener_->callback.setMethod<Server, &Server::doAccept>(this);
  server_->bind(config.port);

  if (config.authentication && config.quotaTable.empty()) {
    LOG_I("Server") << "Warning: Authentication turned on yet quota table is "
                       "unspecified/empty."
                    << std::endl;
    LOG_I("Server")
        << "Warning: All user names would be able to use unlimited data."
        << std::endl;
  }
}

void Server::doAccept() {
  TCPSocket client = server_->accept();

  LOG_I("Center") << "Accepted a client from "
                  << client.getPeerAddress().getHost() << std::endl;

  auto sessionConfig = ServerSessionConfig{config_.encryption,
                                           config_.secret,
                                           config_.paddingTo,
                                           config_.compression,
                                           config_.dataPipeRotationInterval,
                                           config_.authentication,
                                           config_.quotaTable,
                                           config_.mtu};

  auto handler = std::make_unique<ServerSessionHandler>(
      loop_, this, sessionConfig,
      std::make_unique<TCPSocket>(std::move(client)));

  // Trigger to remove finished clients
  auto handlerPtr = handler.get();
  loop_.arm({handler->didEnd()}, [this, handlerPtr]() {
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
} // namespace stun
