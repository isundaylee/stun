#include <Tunnel.h>
#include <Util.h>
#include <UDPPipe.h>
#include <TCPPipe.h>
#include <NetlinkClient.h>
#include <PacketTranslator.h>

#include <ev/ev++.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

using namespace stun;

int main(int argc, char* argv[]) {
  ev::default_loop loop;

  bool server = false;
  if (argc > 1 && (strcmp(argv[1], "server") == 0)) {
    server = true;
  }

  LOG() << "Running as " << (server ? "server" : "client") << std::endl;

  Tunnel tunnel(TunnelType::TUN);
  tunnel.open();
  NetlinkClient netlink;

  netlink.newLink(tunnel.getDeviceName());
  netlink.setLinkAddress(
    tunnel.getDeviceName(),
    server ? "10.100.0.1" : "10.100.0.2",
    server ? "10.100.0.2" : "10.100.0.1"
  );

  UDPPipe udp2;
  TCPPipe tcpServer;

  udp2.open();
  tcpServer.open();

  if (server) {
    udp2.bind(2859);
    tcpServer.bind(2859);
  } else {
    udp2.connect("54.174.137.123", 2859);
  }

  UDPPipe udp(std::move(udp2));

  std::vector<TCPPipe> clients;
  std::vector<std::unique_ptr<PacketTranslator<TCPPacket, TCPPacket>>> echoers;

  tcpServer.onAccept = [&clients, &echoers](TCPPipe&& client) {
    clients.push_back(std::move(client));
    echoers.emplace_back(new PacketTranslator<TCPPacket, TCPPacket>(clients.back().inboundQ, clients.back().outboundQ));
    echoers.back()->transform = [](TCPPacket const& in) {
      return in;
    };
  };

  // These are the TUNNEL translators! ;)
  PacketTranslator<TunnelPacket, UDPPacket> sender(tunnel.inboundQ, udp.outboundQ);
  sender.transform = [](TunnelPacket const& in) {
    UDPPacket out;
    out.size = in.size;
    memcpy(out.buffer, in.buffer, in.size);
    return out;
  };

  PacketTranslator<UDPPacket, TunnelPacket> receiver(udp.inboundQ, tunnel.outboundQ);
  receiver.transform = [](UDPPacket const& in) {
    TunnelPacket out;
    out.size = in.size;
    memcpy(out.buffer, in.buffer, in.size);
    return out;
  };

  loop.run(0);

  return 0;
}
