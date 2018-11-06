#include "stun/ClientSessionHandler.h"

#include <event/SignalCondition.h>
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
  helloBody["mtu"] = config_.mtu;
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

    size_t mtu = config_.mtu;
    if (body.find("mtu") != body.end()) {
      mtu = body["mtu"].template get<size_t>();
    }

    std::vector<IPAddress> dnsPushes;
    if (body.find("dns_pushes") != body.end()) {
      for (auto const& dns : body["dns_pushes"]) {
        dnsPushes.emplace_back(dns.template get<std::string>(),
                               NetworkType::IPv4);
      }
    }

    auto tunnelConfig = ClientTunnelConfig{
        IPAddress(body["client_tunnel_ip"].template get<std::string>(),
                  NetworkType::IPv4),
        IPAddress(body["server_tunnel_ip"].template get<std::string>(),
                  NetworkType::IPv4),
        SubnetAddress(body["server_subnet"].template get<std::string>()),
        mtu,
        config_.subnetsToForward,
        config_.subnetsToExclude,
        dnsPushes,
    };

    auto tunnelPromise = tunnelFactory_(tunnelConfig);

    LOG_I("Session") << "Received config from the server." << std::endl;

    for (auto const& dns : dnsPushes) {
      LOG_I("Session") << "Received DNS server entry: " << dns.toString()
                       << std::endl;
    }

    event::Trigger::arm(
        {tunnelPromise->isReady(), messenger_->outboundQ->canPush()},
        [this, tunnelPromise]() {
          LOG_I("Session") << "Tunnel established." << std::endl;
          dispatcher_.reset(new Dispatcher(tunnelPromise->consume()));

          // TODO: Set up DNS servers

          messenger_->outboundQ->push(Message("config_done", ""));
        });

    return Message::null();
  });

  messenger_->addHandler("new_data_pipe", [this](auto const& message) {
    auto body = message.getBody();

    auto socketAddress =
        SocketAddress{config_.serverAddr.getHost().toString(), body["port"]};
    auto udpPipe = UDPSocket{socketAddress.type};
    udpPipe.connect(socketAddress);

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
    std::string errorMessage = message.getBody().template get<std::string>();
    LOG_I("Session") << "Session ended with error: " << errorMessage
                     << std::endl;
    throw std::runtime_error("Error from server: " + errorMessage);
    return Message::disconnect();
  });
}
} // namespace stun
