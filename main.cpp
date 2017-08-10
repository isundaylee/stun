#include <cxxopts/cxxopts.hpp>

#include <common/Configerator.h>
#include <common/Notebook.h>
#include <common/Util.h>
#include <event/EventLoop.h>
#include <event/Timer.h>
#include <event/Trigger.h>
#include <networking/IPTables.h>
#include <networking/InterfaceConfig.h>
#include <stats/StatsManager.h>
#include <stun/Client.h>
#include <stun/Server.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <regex>
#include <stdexcept>
#include <vector>

using namespace stun;

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
  options.add_option("", "s", "stats", "Enable connection stats logging. You "
                                       "can also give a numeric frequency in "
                                       "milliseconds.",
                     cxxopts::value<int>()->implicit_value("1000"), "");
  options.add_option("", "v", "verbose", "Log more verbosely.",
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

auto parseSubnets(std::string const& key) {
  auto subnets = common::Configerator::getStringArray(key);
  auto results = std::vector<SubnetAddress>{};

  for (auto const& subnet : subnets) {
    results.push_back(SubnetAddress(subnet));
  }

  return results;
}

std::unique_ptr<stun::Server> server;

std::map<std::string, size_t> parseQuotaTable() {
  auto raw =
      common::Configerator::get<std::map<std::string, size_t>>("quotas", {});
  for (auto it = raw.begin(); it != raw.end(); it++) {
    it->second *= 1024 * 1024 * 1024 /* GB -> Bytes */;
  }
  return raw;
}

void setupServer() {
  auto config =
      ServerConfig{kServerPort,
                   networking::SubnetAddress{
                       common::Configerator::get<std::string>("address_pool")},
                   common::Configerator::get<bool>("encryption", true),
                   common::Configerator::get<std::string>("secret", ""),
                   common::Configerator::get<size_t>("padding_to", 0),
                   std::chrono::seconds(common::Configerator::get<size_t>(
                       "data_pipe_rotate_interval", 0)),
                   common::Configerator::get<bool>("authentication", false),
                   parseQuotaTable()};

  server = std::make_unique<stun::Server>(config);
}

std::unique_ptr<stun::Client> client;

void setupClient() {
  auto config = ClientConfig{
      SocketAddress(common::Configerator::getString("server"), kServerPort),
      common::Configerator::get<bool>("encryption", true),
      common::Configerator::get<std::string>("secret", ""),
      common::Configerator::get<size_t>("padding_to", 0),
      std::chrono::seconds(
          common::Configerator::get<size_t>("data_pipe_rotate_interval", 0)),
      common::Configerator::get<std::string>("user", ""),
      parseSubnets("forward_subnets"),
      parseSubnets("excluded_subnets")};

  client.reset(new stun::Client(config));
}

std::string generateNotebookPath(std::string const& configPath) {
  unsigned long hash = 5381;
  for (size_t i = 0; i < configPath.length(); i++) {
    hash = ((hash << 5) + hash) + configPath[i];
  }

  return "/tmp/stun-notebook-" + std::to_string(hash);
}

int main(int argc, char* argv[]) {
  setupAndParseOptions(argc, argv);
  std::string configPath = getConfigPath();
  if (access(configPath.c_str(), F_OK) == -1) {
    generateConfig(configPath);
  }

  common::Configerator config(configPath);
  common::Notebook notebook(generateNotebookPath(configPath));
  event::EventLoop loop;

  LOG_V("Main") << "Config path is: " << configPath << std::endl;
  LOG_V("Main") << "Notebook path is: " << generateNotebookPath(configPath)
                << std::endl;

  std::unique_ptr<event::Timer> statsTimer;
  std::unique_ptr<event::Action> statsDumper;

  if (options.count("stats")) {
    event::Duration statsDumpInerval =
        std::chrono::milliseconds(options["stats"].as<int>());
    statsTimer.reset(new event::Timer{statsDumpInerval});
    statsDumper.reset(new event::Action{{statsTimer->didFire()}});
    statsDumper->callback = [&statsTimer, statsDumpInerval]() {
      stats::StatsManager::dump(LOG_I("Stats"));
      statsTimer->extend(statsDumpInerval);
    };
  }

  std::string role = common::Configerator::getString("role");

  LOG_I("Main") << "Running as " << role << std::endl;

  if (role == "server") {
    setupServer();
  } else {
    setupClient();
  }

  loop.run();

  return 0;
}
