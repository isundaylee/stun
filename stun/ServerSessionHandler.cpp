#include "stun/ServerSessionHandler.h"

#include <stun/Server.h>

#include <event/Action.h>
#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

namespace stun {

using networking::Message;
using networking::Tunnel;
using networking::kTunnelEthernetMTU;
using networking::InterfaceConfig;

static const event::Duration kSessionHandlerRotationGracePeriod = 5000 /* ms */;

ServerSessionHandler::ServerSessionHandler(
    Server* server, ServerConfig config, std::unique_ptr<TCPSocket> commandPipe)
    : server_(server), config_(config),
      messenger_(new Messenger(std::move(commandPipe))),
      didEnd_(new event::BaseCondition()) {
  if (!config_.secret.empty()) {
    messenger_->addEncryptor(
        std::make_unique<crypto::AESEncryptor>(crypto::AESKey(config_.secret)));
  }
  attachHandlers();
}

event::Condition* ServerSessionHandler::didEnd() const { return didEnd_.get(); }

void ServerSessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  event::Trigger::arm({messenger_->didDisconnect()},
                      [this]() { didEnd_->fire(); });

  messenger_->addHandler("hello", [this](auto const& message) {
    // Acquire IP addresses
    auto myTunnelAddr = server_->addrPool->acquire();
    auto peerTunnelAddr = server_->addrPool->acquire();

    // Set up the data tunnel. Data pipes will be set up in a later stage.
    auto tunnel = Tunnel{};
    auto interface = InterfaceConfig{};
    interface.newLink(tunnel.deviceName, kTunnelEthernetMTU);
    interface.setLinkAddress(tunnel.deviceName, myTunnelAddr, peerTunnelAddr);

    dispatcher_.reset(new Dispatcher(std::move(tunnel)));

    // Set up data pipe rotation if it is configured in the server config.
    if (config_.dataPipeRotationInterval != 0) {
      dataPipeRotationTimer_.reset(
          new event::Timer(config_.dataPipeRotationInterval));
      dataPipeRotator_.reset(
          new event::Action({dataPipeRotationTimer_->didFire(),
                             messenger_->outboundQ->canPush()}));
      dataPipeRotator_->callback.setMethod<
          ServerSessionHandler, &ServerSessionHandler::doRotateDataPipe>(this);
    }

    return Message("config", json{{"server_tunnel_ip", myTunnelAddr},
                                  {"client_tunnel_ip", peerTunnelAddr}});
  });

  messenger_->addHandler("config_done", [this](auto const& message) {
    return Message("new_data_pipe", createDataPipe());
  });
}

void ServerSessionHandler::doRotateDataPipe() {
  messenger_->outboundQ->push(Message("new_data_pipe", createDataPipe()));
  dataPipeRotationTimer_->extend(config_.dataPipeRotationInterval);
}

json ServerSessionHandler::createDataPipe() {
  UDPSocket udpPipe;
  int port = udpPipe.bind(0);

  LOG_I("Session") << "Creating a new data pipe." << std::endl;

  // Prepare encryption config
  std::string aesKey;
  if (config_.encryption) {
    aesKey = crypto::AESKey::randomStringKey();
  } else {
    LOG_I("Session") << "Data encryption is disabled per configuration."
                     << std::endl;
  }

  auto ttl = (config_.dataPipeRotationInterval == 0
                  ? 0
                  : config_.dataPipeRotationInterval +
                        kSessionHandlerRotationGracePeriod);
  DataPipe* dataPipe =
      new DataPipe(std::make_unique<UDPSocket>(std::move(udpPipe)), aesKey,
                   config_.paddingTo, ttl);
  dispatcher_->addDataPipe(std::unique_ptr<DataPipe>{dataPipe});

  return json{{"port", port},
              {"aes_key", aesKey},
              {"padding_to_size", config_.paddingTo}};
}
}
