#include "stun/ServerSessionHandler.h"

#include <stun/LossEstimatorHeartbeatService.h>
#include <stun/Server.h>

#include <common/Notebook.h>
#include <event/Action.h>
#include <event/Trigger.h>
#include <networking/InterfaceConfig.h>

#include <chrono>

namespace stun {

using namespace std::chrono_literals;

using networking::InterfaceConfig;
using networking::kTunnelEthernetDefaultMTU;
using networking::Message;
using networking::Tunnel;

static const event::Duration kSessionHandlerQuotaReportInterval = 30min;
static const event::Duration kSessionHandlerRotationGracePeriod = 5s;

static const event::Duration kSessionHandlerQuotaPoliceInterval = 1s;

static const std::set<DataPipeType> kSessionHandlerSupportedDataPipeTypes = {
    DataPipeType::UDP};

class ServerSessionHandler::QuotaReporter {
public:
  QuotaReporter(ServerSessionHandler* session) : session_(session) {
    assertTrue(session->config_.quota != 0,
               "QuotaReporter running on a unlimited session.");

    timer_ = session->loop_.createTimer(0s);
    reporter_ = session_->loop_.createAction(
        "stun::ServerSessionHandler::reporter_",
        {timer_->didFire(), session->messenger_->outboundQ->canPush()});
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
                    toMegaBytesString(session_->config_.priorQuotaUsed +
                                      session_->dispatcher_->bytesDispatched) +
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

class ServerSessionHandler::QuotaPolice {
public:
  QuotaPolice(ServerSessionHandler* session) : session_(session) {
    assertTrue(session->config_.quota != 0,
               "QuotaPolice running on an unlimited session.");

    timer_ = session->loop_.createTimer(0s);
    police_ = session_->loop_.createAction(
        "stun::ServerSessionHandler::police_",
        {timer_->didFire(), session->messenger_->outboundQ->canPush()});
    police_->callback.setMethod<QuotaPolice, &QuotaPolice::doPolice>(this);
  }

private:
  ServerSessionHandler* session_;

  std::unique_ptr<event::Timer> timer_;
  std::unique_ptr<event::Action> police_;

  void doPolice() {
    session_->savePriorQuota();

    if (session_->config_.priorQuotaUsed +
            session_->dispatcher_->bytesDispatched >=
        session_->config_.quota) {
      session_->messenger_->outboundQ->push(
          Message("error", "You have reached your usage quota. Goodbye!"));
      timer_->reset();
    } else {
      timer_->extend(kSessionHandlerQuotaPoliceInterval);
    }
  }

private:
  QuotaPolice(QuotaPolice const& copy) = delete;
  QuotaPolice& operator=(QuotaPolice const& copy) = delete;

  QuotaPolice(QuotaPolice&& move) = delete;
  QuotaPolice& operator=(QuotaPolice&& move) = delete;
};

ServerSessionHandler::ServerSessionHandler(
    event::EventLoop& loop, Server* server, Config config,
    std::unique_ptr<TCPSocket> commandPipe)
    : loop_(loop), server_(server), config_(config),
      peerPublicAddr_(commandPipe->getPeerAddress()),
      messenger_(new Messenger(loop, std::move(commandPipe))),
      didEnd_(loop.createBaseCondition()) {
  if (!config_.secret.empty()) {
    messenger_->addEncryptor(
        std::make_unique<crypto::AESEncryptor>(crypto::AESKey(config_.secret)));
  }
  attachHandlers();
}

void ServerSessionHandler::savePriorQuota() {
  auto& notebook = common::Notebook::getInstance();
  notebook["priorQuotas"][config_.user] =
      config_.priorQuotaUsed + dispatcher_->bytesDispatched;
  notebook.save();
}

ServerSessionHandler::~ServerSessionHandler() {
  if (!!dispatcher_) {
    savePriorQuota();
  }

  if (config_.addrAcquired) {
    server_->addrPool->release(config_.myTunnelAddr);

    if (!config_.authentication ||
        server_->config_.staticHosts.count(config_.user) == 0) {
      // We should only release non-static host addresses
      server_->addrPool->release(config_.peerTunnelAddr);
    }
  }
}

event::Condition* ServerSessionHandler::didEnd() const { return didEnd_.get(); }

void ServerSessionHandler::attachHandlers() {
  // Fire our didEnd() when our command pipe is closed
  loop_.arm({messenger_->didDisconnect()}, [this]() { didEnd_->fire(); });

  messenger_->addHandler("hello", [this](auto const& message) {
    auto body = message.getBody();

    if (body.find("data_pipe_preference") == body.end()) {
      config_.dataPipePreference = {DataPipeType::UDP};
    } else {
      config_.dataPipePreference =
          body["data_pipe_preference"]
              .template get<std::vector<DataPipeType>>();
    }

    if (config_.authentication) {
      if (body.find("user") == body.end()) {
        return Message("error", "No user name provided.");
      }

      config_.user = body["user"].template get<std::string>();
      LOG_I("Session") << getClientLogTag() << ": Client said hello!"
                       << std::endl;

      // Retrieve this user's quota
      if (!config_.quotaTable.empty()) {
        auto it = config_.quotaTable.find(config_.user);
        if (it == config_.quotaTable.end()) {
          LOG_I("Session") << getClientLogTag()
                           << ": Unknown client disconnected." << std::endl;
          return Message("error", "User " + config_.user +
                                      " not allowed on the server.");
        }

        config_.quota = it->second;
        LOG_I("Session") << getClientLogTag() << ": Client has a quota of "
                         << config_.quota << " bytes." << std::endl;

        // Retrieve the user's prior used quota
        auto& notebook = common::Notebook::getInstance();
        if (notebook["priorQuotas"].is_null()) {
          notebook["priorQuotas"] = json({});
        }
        if (notebook["priorQuotas"][config_.user].is_null()) {
          notebook["priorQuotas"][config_.user] = 0;
        }
        config_.priorQuotaUsed = notebook["priorQuotas"][config_.user];
        notebook.save();

        if (config_.quota != 0) {
          quotaReporter_.reset(new QuotaReporter(this));
          quotaPolice_.reset(new QuotaPolice(this));
        }
      }
    }

    // Figure out correct MTU
    if (body.find("mtu") != body.end()) {
      size_t clientMtu = body["mtu"].template get<size_t>();
      config_.mtu = std::min(config_.mtu, clientMtu);
    }

    // Acquire IP addresses
    config_.myTunnelAddr = server_->addrPool->acquire();

    if (config_.authentication &&
        (server_->config_.staticHosts.count(config_.user) != 0)) {
      // This host has a static IP assigned
      config_.peerTunnelAddr = server_->config_.staticHosts[config_.user];
    } else {
      config_.peerTunnelAddr = server_->addrPool->acquire();
    }

    config_.addrAcquired = true;

    // Set up the data tunnel. Data pipes will be set up in a later stage.
    auto tunnel = std::make_unique<Tunnel>(loop_);

#if TARGET_LINUX
    // For now we disable IPv6, since otherwise we might get IPv6 packets on the
    // tunnel that we cannot deal with yet.
    InterfaceConfig::disableIPv6(tunnel->deviceName);
#endif

    InterfaceConfig::newLink(tunnel->deviceName, config_.mtu);
    InterfaceConfig::setLinkAddress(tunnel->deviceName, config_.myTunnelAddr,
                                    config_.peerTunnelAddr);

    dispatcher_.reset(new Dispatcher(loop_, std::move(tunnel)));
    messenger_->addHeartbeatService(
        buildLossEstimatorHeartbeatService(*dispatcher_));

    if (body.find("provided_subnets") != body.end()) {
      for (auto const& subnetString : body["provided_subnets"]) {
        networking::SubnetAddress subnet{
            subnetString.template get<std::string>()};

        LOG_V("Session") << getClientLogTag()
                         << ": Adding routes for client-provided subnet: "
                         << subnet.toString() << std::endl;

        auto route = networking::Route{
            subnet, networking::RouteDestination{config_.peerTunnelAddr}};

        try {
          InterfaceConfig::newRoute(route);
        } catch (std::runtime_error const& ex) {
          // TODO: Use some more specific exception type?
          LOG_E("Session")
              << getClientLogTag()
              << ": Failed to add a route for client-provided subnet: "
              << ex.what() << std::endl;
        }

        // TODO: Upon disconnection, these routes would be removed along with
        // the tunnel interface. Should we still remove them manually for good
        // hygiene?
      }
    }

    // Set up data pipe rotation if it is configured in the server config.
    if (config_.dataPipeRotationInterval != 0s) {
      dataPipeRotationTimer_ =
          loop_.createTimer(config_.dataPipeRotationInterval);
      dataPipeRotator_ =
          loop_.createAction("stun::ServerSessionHandler::dataPipeRotator_",
                             {dataPipeRotationTimer_->didFire(),
                              messenger_->outboundQ->canPush()});
      dataPipeRotator_->callback.setMethod<
          ServerSessionHandler, &ServerSessionHandler::doRotateDataPipe>(this);
    }

    return Message("config",
                   json{
                       {"server_tunnel_ip", config_.myTunnelAddr},
                       {"client_tunnel_ip", config_.peerTunnelAddr},
                       {"server_subnet", server_->config_.addressPool},
                       {"mtu", config_.mtu},
                       {"dns_pushes", server_->config_.dnsPushes},
                   });
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
  LOG_V("Session") << getClientLogTag() << ": Creating a new data pipe."
                   << std::endl;

  // Prepare encryption config
  std::string aesKey;
  if (config_.encryption) {
    aesKey = crypto::AESKey::randomStringKey();
  } else {
    LOG_V("Session") << getClientLogTag()
                     << ": Data encryption is disabled per configuration."
                     << std::endl;
  }

  auto ttl = (config_.dataPipeRotationInterval == 0s
                  ? 0s
                  : config_.dataPipeRotationInterval +
                        kSessionHandlerRotationGracePeriod);

  auto dataPipeType = DataPipeType::UDP;
  for (auto preferredType : config_.dataPipePreference) {
    if (kSessionHandlerSupportedDataPipeTypes.count(preferredType) != 0) {
      dataPipeType = preferredType;
      break;
    }
  }

  auto coreConfig = [dataPipeType]() -> DataPipe::CoreConfig {
    switch (dataPipeType) {
    case DataPipeType::UDP:
      return UDPCoreDataPipe::ServerConfig{};
      break;
    case DataPipeType::TCP:
      notImplemented("TCP data pipe not supported yet.");
      break;
    }
  }();

  auto dataPipeConfig = DataPipe::Config{
      coreConfig, DataPipe::CommonConfig{aesKey, config_.paddingTo,
                                         config_.compression, ttl}};
  auto dataPipe = std::make_unique<DataPipe>(loop_, std::move(dataPipeConfig));
  auto port = dynamic_cast<UDPCoreDataPipe&>(dataPipe->getCore()).getPort();
  dispatcher_->addDataPipe(std::move(dataPipe));

  return json{{"type", dataPipeType},
              {"port", port},
              {"aes_key", aesKey},
              {"padding_to_size", config_.paddingTo},
              {"compression", config_.compression}};
}

std::string ServerSessionHandler::getClientLogTag() const {
  if (config_.authentication) {
    return config_.user;
  } else {
    return peerPublicAddr_.getHost().toString();
  }
}

} // namespace stun
