#include <cxxopts/cxxopts.hpp>

#include <common/Configerator.h>
#include <common/Util.h>
#include <event/EventLoop.h>
#include <event/Timer.h>
#include <event/Trigger.h>
#include <networking/IPTables.h>
#include <networking/InterfaceConfig.h>
#include <stats/StatsManager.h>
#include <stun/CommandCenter.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <vector>

using namespace stun;

const int kReconnectDelayInterval = 5000 /* ms */;
const int kServerPort = 2859;

static const std::string serverConfigTemplate = R"(
{
  "role": "server",
  "secret": "SECRET",
  "address_pool": "10.100.0.0/24",
  "padding_to": 1000,
  "data_pipe_rotate_interval": 60
}
)";

static const std::string clientConfigTemplate = R"(
{
  "role": "client",
  "server": "SERVER_IP",
  "secret": "SECRET",
  "forward_subnets": [
    "0.0.0.0/1",
    "128.0.0.0/1"
  ],
  "excluded_subnets": [
  ]
}
)";

cxxopts::Options options("stun", "A simple layer-3 network packet tunnel.");

std::string getConfigPath() {
  if (options.count("config")) {
    return options["config"].as<std::string>();
  }

  const char* homeDir = getenv("HOME");
  assertTrue(homeDir != NULL, "Cannot get $HOME. You must specify an explicit "
                              "config path with the -c option.");

  return std::string(homeDir) + "/.stunrc";
}

void generateConfig(std::string path) {
  std::string role, serverAddr, secret;

  std::cout << "I will help you create a stun config file at " << path << "."
            << std::endl;

  // Prompt the user for role
  while (role != "server" && role != "client") {
    std::cout << "Is this a client or a server? ";
    std::getline(std::cin, role);
  }

  // Prompt the user for server address if we're a client
  if (role == "client") {
    while (serverAddr.empty()) {
      std::cout << "What is the server's address? ";
      std::getline(std::cin, serverAddr);
    }
  }

  // Prompt the user for the secret
  while (secret.empty()) {
    std::cout << "What is the secret (passcode) for the server? ";
    std::getline(std::cin, secret);
  }

  std::string content =
      (role == "server" ? serverConfigTemplate : clientConfigTemplate);
  content = std::regex_replace(content, std::regex("SERVER_IP"), serverAddr);
  content = std::regex_replace(content, std::regex("SECRET"), secret);

  std::ofstream config(path);
  config.write(content.c_str(), content.length());
}

void setupAndParseOptions(int argc, char* argv[]) {
  options.add_option("", "c", "config",
                     "Path to the config file. Default is ~/.stunrc.",
                     cxxopts::value<std::string>(), "");
  options.add_option("", "v", "verbose", "Logs more verbosely.",
                     cxxopts::value<bool>(), "");
  options.add_option("", "h", "help", "Print help and usage info.",
                     cxxopts::value<bool>(), "");

  try {
    options.parse(argc, argv);
  } catch (cxxopts::OptionException const& ex) {
    std::cout << "Cannot parse options: " << ex.what() << std::endl;
    std::cout << std::endl << options.help() << std::endl;
    exit(1);
  }

  if (options.count("help")) {
    std::cout << options.help() << std::endl;
    exit(1);
  }

  if (options.count("verbose")) {
    common::Logger::getDefault("").setLoggingThreshold(
        common::LogLevel::VERBOSE);
  } else {
    common::Logger::getDefault("").setLoggingThreshold(common::LogLevel::INFO);
  }
}

int main(int argc, char* argv[]) {
  setupAndParseOptions(argc, argv);
  std::string configPath = getConfigPath();
  if (access(configPath.c_str(), F_OK) == -1) {
    generateConfig(configPath);
  }

  common::Configerator config(configPath);
  event::EventLoop loop;

  std::string role = common::Configerator::getString("role");

  LOG_I("Main") << "Running as " << role << std::endl;
  CommandCenter center;
  event::BaseCondition shouldMonitorDisconnect;
  event::Action disconnectMonitor(
      {&shouldMonitorDisconnect, center.didDisconnect()});
  event::Timer reconnectTimer{};
  event::Action reconnector({reconnectTimer.didFire()});

  if (role == "server") {
    std::string addressPool = common::Configerator::getString("address_pool");
    networking::SubnetAddress subnet(addressPool);
    networking::IPTables::masquerade(subnet);

    center.serve(kServerPort);
  } else {
    std::string server = common::Configerator::getString("server");
    center.connect(server, kServerPort);
    shouldMonitorDisconnect.fire();

    disconnectMonitor.callback = [&shouldMonitorDisconnect, &reconnectTimer]() {
      LOG_I("Main") << "Will reconnect in " << kReconnectDelayInterval << " ms."
                    << std::endl;
      shouldMonitorDisconnect.arm();
      reconnectTimer.reset(kReconnectDelayInterval);
    };

    reconnector.callback = [&shouldMonitorDisconnect, &reconnectTimer, &center,
                            server]() {
      LOG_I("Main") << "Reconnecting..." << std::endl;
      center.connect(server, kServerPort);
      shouldMonitorDisconnect.fire();
      reconnectTimer.reset();
    };
  }

  loop.run();

  return 0;
}
