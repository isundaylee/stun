#include "CommandCenter.h"

#include <Messenger.h>

#include <Util.h>

namespace stun {

void CommandCenter::serve(int port) {
  commandServer.onAccept = [this](TCPPipe&& client) {
    LOG() << "A new client connected" << std::endl;

    client.name = "COMMAND-" + std::to_string(servers.size());
    servers.emplace_back(servers.size(), std::move(client));
    servers.back().start();
  };

  commandServer.open();
  commandServer.bind(port);
}

void CommandCenter::connect(std::string const& host, int port) {
  TCPPipe toServer;
  toServer.name = "COMMAND";
  toServer.open();
  toServer.connect(host, port);

  client.reset(new ClientHandler(std::move(toServer)));
  client->start();
}

}
