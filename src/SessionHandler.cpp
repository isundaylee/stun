#include "SessionHandler.h"

#include <CommandCenter.h>

namespace stun {

using namespace networking;

SessionHandler::SessionHandler(CommandCenter* center, bool isServer,
                               std::string serverAddr, size_t clientIndex,
                               TCPPipe&& client)
    : center_(center), isServer_(isServer), serverAddr_(serverAddr),
      clientIndex(clientIndex), commandPipe_(new TCPPipe(std::move(client))),
      messenger_(new Messenger(*commandPipe_)) {
  attachHandler();
}

SessionHandler::SessionHandler(SessionHandler&& move)
    : commandPipe_(std::move(move.commandPipe_)),
      dataPipe_(std::move(move.dataPipe_)),
      messenger_(std::move(move.messenger_)), tun_(std::move(move.tun_)),
      sender_(std::move(move.sender_)), receiver_(std::move(move.receiver_)),
      clientIndex(std::move(move.clientIndex)),
      center_(std::move(move.center_)) {
  attachHandler();
}

SessionHandler& SessionHandler::operator=(SessionHandler&& move) {
  using std::swap;
  swap(commandPipe_, move.commandPipe_);
  swap(dataPipe_, move.dataPipe_);
  swap(messenger_, move.messenger_);
  swap(tun_, move.tun_);
  swap(sender_, move.sender_);
  swap(receiver_, move.receiver_);
  swap(clientIndex, move.clientIndex);
  swap(center_, move.center_);
  attachHandler();
  return *this;
}

void SessionHandler::start() {
  messenger_->start();

  if (!isServer_) {
    assertTrue(
        messenger_->canSend()->value,
        "How can I not be able to send at the very start of a connection?");
    messenger_->send(Message("hello", ""));
  }
}

void SessionHandler::attachHandler() {
  messenger_->handler = [this](Message const& message) {
    return (isServer_ ? handleMessageFromClient(message)
                      : handleMessageFromServer(message));
  };
}

void SessionHandler::createDataTunnel(std::string const& myAddr,
                                      std::string const& peerAddr) {
  // Establish tunnel
  tun_.reset(new Tunnel(TunnelType::TUN));
  tun_->open();
  NetlinkClient client;
  client.newLink(tun_->getDeviceName());
  client.setLinkAddress(tun_->getDeviceName(), myAddr, peerAddr);

  // Configure sender and receiver
  sender_.reset(new PacketTranslator<TunnelPacket, UDPPacket>(
      tun_->inboundQ.get(), dataPipe_->outboundQ.get()));
  sender_->transform = [](TunnelPacket const& in) {
    UDPPacket out;
    out.size = in.size;
    memcpy(out.buffer, in.buffer, in.size);
    return out;
  };
  sender_->start();

  receiver_.reset(new PacketTranslator<UDPPacket, TunnelPacket>(
      dataPipe_->inboundQ.get(), tun_->outboundQ.get()));
  receiver_->transform = [](UDPPacket const& in) {
    TunnelPacket out;
    out.size = in.size;
    memcpy(out.buffer, in.buffer, in.size);
    return out;
  };
  receiver_->start();
}

Message SessionHandler::handleMessageFromClient(Message const& message) {
  std::string type = message.getType();

  if (type == "hello") {
    // Set up data pipe
    dataPipe_.reset(new UDPPipe());
    dataPipe_->name = "DATA-" + std::to_string(clientIndex);
    dataPipe_->shouldOutputStats = true;
    dataPipe_->open();
    int port = dataPipe_->bind(0);

    // Acquire IP addresses
    std::string const& myAddr = center_->addrPool->acquire();
    std::string const& peerAddr = center_->addrPool->acquire();
    LOG() << "Acquired IP address: mine = " << myAddr << ", peer = " << peerAddr
          << std::endl;

    createDataTunnel(myAddr, peerAddr);

    return Message(
        "config",
        json{
            {"server_ip", myAddr}, {"client_ip", peerAddr}, {"data_port", port},
        });
  } else {
    unreachable("Unrecognized client message type: " + type);
  }
}

Message SessionHandler::handleMessageFromServer(Message const& message) {
  std::string type = message.getType();
  json body = message.getBody();
  std::vector<Message> replies;

  if (type == "config") {
    // Create data pipe
    dataPipe_.reset(new UDPPipe());
    dataPipe_->name = "DATA-" + std::to_string(clientIndex);
    dataPipe_->shouldOutputStats = true;
    dataPipe_->open();
    dataPipe_->connect(serverAddr_, body["data_port"]);

    createDataTunnel(body["client_ip"], body["server_ip"]);
    return Message::null();
  } else {
    unreachable("Unrecognized server message type: " + type);
  }
}
}
