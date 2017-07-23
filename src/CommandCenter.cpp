#include "CommandCenter.h"

#include <Messenger.h>
#include <Util.h>

#include <algorithm>

namespace stun {

int CommandCenter::numClients = 0;

void CommandCenter::serve(int port) {
  commandServer.onAccept = [this](TCPPipe&& client) {
    LOG() << "A new client connected" << std::endl;

    size_t clientIndex = numClients;
    numClients ++;

    client.onClose = [this, clientIndex]() {
      auto it = std::find_if(servers.begin(), servers.end(), [clientIndex](ServerHandler const& server) {
        return server.clientIndex == clientIndex;
      });

      assertTrue(it != servers.end(), "Cannot find the client to remove.");
      servers.erase(it);
    };

    client.name = "COMMAND-" + std::to_string(clientIndex);
    servers.emplace_back(clientIndex, std::move(client));
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
