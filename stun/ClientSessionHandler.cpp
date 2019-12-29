#include "stun/ClientSessionHandler.h"

#include "stun/LossEstimatorHeartbeatService.h"

#include <event/SignalCondition.h>
#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

#include <chrono>

namespace stun {

using namespace std::chrono_literals;

ClientSessionHandler::ClientSessionHandler(
    event::EventLoop& loop, ClientConfig config,
    std::unique_ptr<TCPSocket> commandPipe, TunnelFactory tunnelFactory)
    : loop_(loop), config_(config), tunnelFactory_(tunnelFactory),
      messenger_(new Messenger(loop, std::move(commandPipe))),
      didEnd_(loop.createBaseCondition()) {
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
  helloBody["provided_subnets"] = json::array();
  for (auto const& subnet : config_.subnetsProvided) {
    helloBody["provided_subnets"].push_back(subnet.toString());
  }
  helloBody["data_pipe_preference"] = config_.dataPipePreference;
  messenger_->outboundQ->push(Message("hello", helloBody));

  attachHandlers();
}

event::Condition* ClientSessionHandler::didEnd() const { return didEnd_.get(); }

void ClientSessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  loop_.arm("stun::ClientSessionHandler::messengerDisconnectTrigger",
            {messenger_->didDisconnect()}, [this]() { didEnd_->fire(); });

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
        config_.acceptDNSPushes,
    };

    auto tunnelPromise = tunnelFactory_(tunnelConfig);

    LOG_I("Session") << "Received config from the server." << std::endl;

    for (auto const& dns : dnsPushes) {
      LOG_I("Session") << "Received DNS server entry: " << dns.toString()
                       << std::endl;
    }

    loop_.arm("stun::ClientSessionHandler::tunnelPromiseReadyTrigger",
              {tunnelPromise->isReady(), messenger_->outboundQ->canPush()},
              [this, tunnelPromise]() {
                LOG_I("Session") << "Tunnel established." << std::endl;
                dispatcher_.reset(
                    new Dispatcher(loop_, tunnelPromise->consume()));
                messenger_->addHeartbeatService(
                    buildLossEstimatorHeartbeatService(*dispatcher_));

                // TODO: Set up DNS servers

                messenger_->outboundQ->push(Message("config_done", ""));
              });

    return Message::null();
  });

  messenger_->addHandler("new_data_pipe", [this](auto const& message) {
    auto body = message.getBody();

    auto socketAddress =
        SocketAddress{config_.serverAddr.getHost().toString(), body["port"]};

    auto dataPipeType = DataPipeType::UDP;
    if (body.find("type") != body.end()) {
      dataPipeType = body["type"];
    }

    auto coreConfig = [dataPipeType, &socketAddress]() -> DataPipe::CoreConfig {
      switch (dataPipeType) {
      case DataPipeType::UDP:
        return UDPCoreDataPipe::ClientConfig{std::move(socketAddress)};
      case DataPipeType::TCP:
        return TCPCoreDataPipe::ClientConfig{std::move(socketAddress)};
      }
    }();
    auto dataPipeConfig = DataPipe::Config{
        coreConfig,
        DataPipe::CommonConfig{body["aes_key"], body["padding_to_size"],
                               body["compression"], 0s}};
    auto dataPipe =
        std::make_unique<DataPipe>(loop_, std::move(dataPipeConfig));

    dispatcher_->addDataPipe(std::move(dataPipe));

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
