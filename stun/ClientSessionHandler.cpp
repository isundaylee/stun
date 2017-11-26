#include "stun/ClientSessionHandler.h"

#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

#include <chrono>

namespace stun {

using namespace std::chrono_literals;

ClientSessionHandler::ClientSessionHandler(
    ClientConfig config, std::unique_ptr<TCPSocket> commandPipe,
    TunnelFactory tunnelFactory)
    : config_(config), tunnelFactory_(tunnelFactory),
      messenger_(new Messenger(std::move(commandPipe))),
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

    auto tunnelConfig = ClientTunnelConfig{
        IPAddress(body["client_tunnel_ip"].template get<std::string>(),
                  NetworkType::IPv4),
        IPAddress(body["server_tunnel_ip"].template get<std::string>(),
                  NetworkType::IPv4),
        SubnetAddress(body["server_subnet"].template get<std::string>()),
        kTunnelEthernetMTU,
        config_.subnetsToForward,
        config_.subnetsToExclude,
    };

    auto tunnelPromise = tunnelFactory_(tunnelConfig);

    LOG_I("Session") << "Received config from the server." << std::endl;

    event::Trigger::arm(
        {tunnelPromise->isReady(), messenger_->outboundQ->canPush()},
        [this, tunnelPromise]() {
          LOG_I("Session") << "Tunnel established." << std::endl;
          dispatcher_.reset(new Dispatcher(tunnelPromise->consume()));

          messenger_->outboundQ->push(Message("config_done", ""));
        });

    return Message::null();
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
}
