#include "UDPConnection.h"

namespace stun {

UDPConnection::UDPConnection(int socket) {
  socket_ = socket;
}

UDPConnection::UDPConnection(std::string const& host, int port) {
}

}
