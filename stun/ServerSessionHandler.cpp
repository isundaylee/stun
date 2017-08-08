#include "stun/ServerSessionHandler.h"

#include <stun/Server.h>

#include <event/Action.h>
#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

#include <chrono>

namespace stun {

using namespace std::chrono_literals;

using networking::Message;
using networking::Tunnel;
using networking::kTunnelEthernetMTU;
using networking::InterfaceConfig;

static const event::Duration kSessionHandlerQuotaReportInterval = 30min;
static const event::Duration kSessionHandlerRotationGracePeriod = 5s;

class ServerSessionHandler::QuotaReporter {
public:
  QuotaReporter(ServerSessionHandler* session) : session_(session) {
    timer_.reset(new event::Timer(0s));
    reporter_.reset(new event::Action(
        {timer_->didFire(), session->messenger_->outboundQ->canPush()}));
    reporter_->callback.setMethod<QuotaReporter, &QuotaReporter::doReport>(
        this);
  }

private:
  ServerSessionHandler* session_;

  std::unique_ptr<event::Timer> timer_;
  std::unique_ptr<event::Action> reporter_;

  std::string toMegaBytesString(size_t bytes) {
    double megabytes = (1.0 / 1024 / 1024) * bytes;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << megabytes << " MB";
    return ss.str();
  }

  void doReport() {
    session_->messenger_->outboundQ->push(
        Message("message",
                "You have used " +
                    toMegaBytesString(session_->dispatcher_->bytesDispatched) +
                    +" out of your quota of " +
                    toMegaBytesString(session_->config_.quota) + "."));
    timer_->extend(kSessionHandlerQuotaReportInterval);
  }

private:
  QuotaReporter(QuotaReporter const& copy) = delete;
  QuotaReporter& operator=(QuotaReporter const& copy) = delete;

  QuotaReporter(QuotaReporter&& move) = delete;
  QuotaReporter& operator=(QuotaReporter&& move) = delete;
};

ServerSessionHandler::ServerSessionHandler(
    Server* server, ServerSessionConfig config,
    std::unique_ptr<TCPSocket> commandPipe)
    : server_(server), config_(config),
      messenger_(new Messenger(std::move(commandPipe))),
      didEnd_(new event::BaseCondition()) {
  if (!config_.secret.empty()) {
    messenger_->addEncryptor(
        std::make_unique<crypto::AESEncryptor>(crypto::AESKey(config_.secret)));
  }
  attachHandlers();
}

ServerSessionHandler::~ServerSessionHandler() = default;

event::Condition* ServerSessionHandler::didEnd() const { return didEnd_.get(); }

void ServerSessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  event::Trigger::arm({messenger_->didDisconnect()},
                      [this]() { didEnd_->fire(); });

  messenger_->addHandler("hello", [this](auto const& message) {
    if (config_.authentication) {
      auto body = message.getBody();

      if (body.find("user") == body.end()) {
        return Message("error", "No user name provided.");
      }

      config_.user = body["user"].template get<std::string>();
      LOG_I("Session") << "Client " << config_.user << " said hello!"
                       << std::endl;

      // Retrieve this user's quota
      if (!config_.quotaTable.empty()) {
        auto it = config_.quotaTable.find(config_.user);
        if (it == config_.quotaTable.end()) {
          return Message(
              "error", "User " + config_.user + " not allowed on the server.");
        }

        config_.quota = it->second;
        LOG_V("Session") << "Client " << config_.user << " has a quota of "
                         << config_.quota << " bytes." << std::endl;

        if (config_.quota != 0) {
          quotaReporter_.reset(new QuotaReporter(this));
        }
      }
    }

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
    if (config_.dataPipeRotationInterval != 0s) {
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

  auto ttl = (config_.dataPipeRotationInterval == 0s
                  ? 0s
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
