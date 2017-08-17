#pragma once

#include <third-party/json/json.hpp>

#include <stun/Dispatcher.h>

#include <networking/IPAddressPool.h>
#include <networking/Messenger.h>
#include <networking/TCPSocket.h>

namespace stun {

using json = nlohmann::json;

using networking::TCPSocket;
using networking::Messenger;
using networking::SubnetAddress;
using networking::IPAddress;

struct ServerSessionConfig {
public:
  bool encryption;
  std::string secret;
  size_t paddingTo;
  bool compression;
  event::Duration dataPipeRotationInterval;
  bool authentication;
  std::map<std::string, size_t> quotaTable;

  std::string user = "";
  size_t quota = 0;
  size_t priorQuotaUsed = 0;
  IPAddress myTunnelAddr;
  IPAddress peerTunnelAddr;
};

class Server;

class ServerSessionHandler {
public:
  ServerSessionHandler(Server* server, ServerSessionConfig config,
                       std::unique_ptr<TCPSocket> commandPipe);
  ~ServerSessionHandler();

  event::Condition* didEnd() const;

private:
  Server* server_;
  ServerSessionConfig config_;

  class QuotaReporter;
  class QuotaPolice;

  std::unique_ptr<Messenger> messenger_;
  std::unique_ptr<Dispatcher> dispatcher_;

  std::unique_ptr<event::Timer> dataPipeRotationTimer_;
  std::unique_ptr<event::Action> dataPipeRotator_;

  std::unique_ptr<QuotaReporter> quotaReporter_;
  std::unique_ptr<QuotaPolice> quotaPolice_;

  std::unique_ptr<event::BaseCondition> didEnd_;

  void attachHandlers();
  json createDataPipe();
  void doRotateDataPipe();
  void savePriorQuota();
};
}
