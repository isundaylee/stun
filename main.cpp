#include <cxxopts/cxxopts.hpp>

#include <Metadata.h>

#include <common/Configerator.h>
#include <common/Notebook.h>
#include <common/Util.h>
#include <event/EventLoop.h>
#include <event/Timer.h>
#include <event/Trigger.h>
#include <flutter/Server.h>
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

const int kDefaultServerPort = 2859;

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
  "user": "USER",
  "secret": "SECRET",
  "forward_subnets": [
    "0.0.0.0/1",
    "128.0.0.0/1"
  ],
  "excluded_subnets": [
  ],
  "accept_dns_pushes": ACCEPT_DNS_PUSHES
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

std::string getWizardInput(char const* message,
                           std::function<bool(std::string const&)> validator) {
  auto input = std::string{};

  do {
    std::cout << message;
    std::getline(std::cin, input);
  } while (!validator(input));

  return input;
}

void generateConfig(std::string path) {
  std::string role, serverAddr, user, acceptDNSPushes, secret;

  std::cout << "I will help you create a stun config file at " << path << "."
            << std::endl;

  // Prompt the user for role
  role =
      getWizardInput("Is this a client or a server? ", [](auto const& value) {
        return value == "server" || value == "client";
      });

  // Prompt the user for server address and client name if we're a client
  if (role == "client") {
    serverAddr =
        getWizardInput("What is the server's address? ",
                       [](auto const& value) { return !value.empty(); });
    user = getWizardInput(
        "What is your assigned user name? (May be left empty for an "
        "unauthenticated server) ",
        [](auto) { return true; });
    acceptDNSPushes = getWizardInput(
        "Do you want to automatically apply DNS settings that the "
        "server sends if available? (yes or no) ",
        [](auto const& value) { return (value == "yes") || (value == "no"); });
  }

  // Prompt the user for the secret
  secret = getWizardInput("What is the secret (passcode) for the server? ",
                          [](auto const& value) { return !value.empty(); });

  std::string content =
      (role == "server" ? serverConfigTemplate : clientConfigTemplate);
  content = std::regex_replace(content, std::regex("SERVER_IP"), serverAddr);
  content = std::regex_replace(content, std::regex("ACCEPT_DNS_PUSHES"),
                               acceptDNSPushes == "yes" ? "true" : "false");
  content = std::regex_replace(content, std::regex("SECRET"), secret);
  content = std::regex_replace(content, std::regex("USER"), user);

  std::ofstream config(path);
  config.write(content.c_str(), content.length());
}

void setupAndParseOptions(int argc, char* argv[]) {
  options.add_option("", "c", "config",
                     "Path to the config file. Default is ~/.stunrc.",
                     cxxopts::value<std::string>(), "");
  options.add_option("", "w", "wizard",
                     "Show config wizard even if a config file exists already.",
                     cxxopts::value<bool>(), "");
  options.add_option("", "f", "flutter",
                     "Starts a flutter server on given port to export stats.",
                     cxxopts::value<int>()->implicit_value("4999"), "");
  options.add_option(
      "", "s", "stats",
      "Enable connection stats logging. You "
      "can also give a numeric frequency in "
      "milliseconds.",
      cxxopts::value<int>()->implicit_value("1000")->default_value("1000"), "");
  options.add_option("", "v", "verbose", "Log more verbosely.",
                     cxxopts::value<bool>(), "");
  options.add_option("", "", "very-verbose", "Log very verbosely.",
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

  if (options.count("very-verbose")) {
    common::Logger::getDefault("").setLoggingThreshold(
        common::LogLevel::VERY_VERBOSE);
  } else if (options.count("verbose")) {
    common::Logger::getDefault("").setLoggingThreshold(
        common::LogLevel::VERBOSE);
  } else {
    common::Logger::getDefault("").setLoggingThreshold(common::LogLevel::INFO);
  }
}

auto parseSubnets(std::string const& key) {
  if (!common::Configerator::hasKey(key)) {
    return std::vector<SubnetAddress>{};
  }

  auto subnets = common::Configerator::getStringArray(key);
  auto results = std::vector<SubnetAddress>{};

  for (auto const& subnet : subnets) {
    results.push_back(SubnetAddress(subnet));
  }

  return results;
}

auto parseStaticHosts() {
  auto result = std::map<std::string, IPAddress>{};
  auto staticHosts = common::Configerator::getJSON()["static_hosts"];

  // FIXME: Remove workaround of https://github.com/nlohmann/json/issues/600
  // once json 3.0.0 is released.
  for (auto const& entry : json::iterator_wrapper(staticHosts)) {
    result[entry.key()] = entry.value();
  }

  return result;
}

std::map<std::string, size_t> parseQuotaTable() {
  auto raw =
      common::Configerator::get<std::map<std::string, size_t>>("quotas", {});
  for (auto it = raw.begin(); it != raw.end(); it++) {
    it->second *= 1024 * 1024 * 1024 /* GB -> Bytes */;
  }
  return raw;
}

std::unique_ptr<stun::Server> setupServer(event::EventLoop& loop,
                                          std::string getServerConfigID) {
  auto config = ServerConfig{
      getServerConfigID,
      common::Configerator::get<int>("port", kDefaultServerPort),
      networking::SubnetAddress{
          common::Configerator::get<std::string>("address_pool")},
      common::Configerator::get<std::string>("masquerade_output_interface", ""),
      common::Configerator::get<bool>("encryption", true),
      common::Configerator::get<std::string>("secret", ""),
      common::Configerator::get<size_t>("padding_to", 0),
      common::Configerator::get<bool>("compression", false),
      std::chrono::seconds(
          common::Configerator::get<size_t>("data_pipe_rotate_interval", 0)),
      common::Configerator::get<bool>("authentication", false),
      common::Configerator::get<size_t>("mtu",
                                        networking::kTunnelEthernetDefaultMTU),
      parseQuotaTable(),
      parseStaticHosts(),
      common::Configerator::get<std::vector<networking::IPAddress>>(
          "dns_pushes", {}),
  };

  return std::make_unique<stun::Server>(loop, config);
}

std::unique_ptr<stun::Client> setupClient(event::EventLoop& loop) {
  auto config = ClientConfig{
      SocketAddress(common::Configerator::getString("server"),
                    common::Configerator::get<int>("port", kDefaultServerPort)),
      common::Configerator::get<bool>("encryption", true),
      common::Configerator::get<std::string>("secret", ""),
      common::Configerator::get<size_t>("padding_to", 0),
      std::chrono::seconds(
          common::Configerator::get<size_t>("data_pipe_rotate_interval", 0)),
      common::Configerator::get<std::string>("user", ""),
      common::Configerator::get<size_t>("mtu",
                                        networking::kTunnelEthernetDefaultMTU),
      common::Configerator::get<bool>("accept_dns_pushes", false),
      parseSubnets("forward_subnets"),
      parseSubnets("excluded_subnets"),
      parseSubnets("provided_subnets")};

  return std::make_unique<stun::Client>(loop, config);
}

std::unique_ptr<flutter::Server> setupFlutterServer(event::EventLoop& loop) {
  if (options.count("flutter") == 0) {
    return nullptr;
  }

  auto port = options["flutter"].as<int>();
  auto flutterServerConfig = flutter::ServerConfig{port};

  return std::make_unique<flutter::Server>(loop, flutterServerConfig);
}

std::string getServerConfigID(std::string const& configPath) {
  unsigned long hash = 5381;
  for (size_t i = 0; i < configPath.length(); i++) {
    hash = ((hash << 5) + hash) + configPath[i];
  }

  return std::to_string(hash);
}

std::string generateNotebookPath(std::string const& configPath) {
  return "/tmp/stun-notebook-" + getServerConfigID(configPath);
}

int main(int argc, char* argv[]) {
  event::EventLoop loop;

#define XSTRINGIFY(str) #str
#define STRINGIFY(str) XSTRINGIFY(str)
  auto buildFlavor = STRINGIFY(STUN_BUILD_FLAVOR);
#undef STRINGIFY
#undef XSTRINGIFY

  auto gitVersion = kGitCommitId;

  if (kGitDirtyStatus) {
    gitVersion += " dirty";
  }

  auto version = kVersion + " (" + buildFlavor + " build, " + gitVersion + ")";

  LOG_I("Main") << "Running version " << version << std::endl;

  setupAndParseOptions(argc, argv);
  std::string configPath = getConfigPath();
  if (options.count("wizard") || access(configPath.c_str(), F_OK) == -1) {
    generateConfig(configPath);
  }

  common::Configerator config(configPath);
  common::Notebook notebook(generateNotebookPath(configPath));

  LOG_V("Main") << "Config path is: " << configPath << std::endl;
  LOG_V("Main") << "Notebook path is: " << generateNotebookPath(configPath)
                << std::endl;

  std::unique_ptr<event::Timer> statsTimer;
  std::unique_ptr<event::Action> statsDumper;

  // Sets up stats collection
  event::Duration statsDumpInerval =
      std::chrono::milliseconds(options["stats"].as<int>());
  statsTimer = loop.createTimer(statsDumpInerval);
  statsDumper = loop.createAction({statsTimer->didFire()});
  statsDumper->callback = [&statsTimer, statsDumpInerval]() {
    stats::StatsManager::collect();
    statsTimer->extend(statsDumpInerval);
  };

  stats::StatsManager::subscribe([](auto const& data) {
    stats::StatsManager::dump(LOG_V("Stats"), data);
  });

  auto flutterServer = setupFlutterServer(loop);

  std::string role = common::Configerator::getString("role");

  LOG_I("Main") << "Running as " << role << std::endl;

  std::unique_ptr<stun::Server> server;
  std::unique_ptr<stun::Client> client;

  if (role == "server") {
    server = setupServer(loop, getServerConfigID(configPath));
  } else {
    client = setupClient(loop);
  }

  loop.run();

  return 0;
}
