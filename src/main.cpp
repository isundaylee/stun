#include <Tunnel.h>
#include <Util.h>
#include <UDPPipe.h>
#include <NetlinkClient.h>
#include <PacketTranslator.h>

#include <ev/ev++.h>
#include <unistd.h>

#include <iostream>

using namespace stun;

int main(int argc, char* argv[]) {
  ev::default_loop loop;

  bool server = false;
  if (argc > 1 && (strcmp(argv[1], "server") == 0)) {
    server = true;
  }

  LOG() << "Running as " << (server ? "server" : "client") << std::endl;

  Tunnel tunnel(TunnelType::TUN);
  NetlinkClient netlink;

  netlink.newLink(tunnel.getDeviceName());
  netlink.setLinkAddress(
    tunnel.getDeviceName(),
    server ? "10.100.0.1" : "10.100.0.2",
    server ? "10.100.0.2" : "10.100.0.1"
  );

  UDPPipe udp;

  if (server) {
    udp.bind(2859);
  } else {
    udp.connect("54.174.137.123", 2859);
  }

  // These are the TUNNEL translators! ;)
  PacketTranslator<TunnelPacket, UDPPacket> sender(tunnel.inboundQ, udp.outboundQ);
  sender.transform = [](TunnelPacket const& in) {
    UDPPacket out;
    out.size = in.size;
    memcpy(out.data, in.buffer, in.size);
    return out;
  };

  PacketTranslator<UDPPacket, TunnelPacket> receiver(udp.inboundQ, tunnel.outboundQ);
  receiver.transform = [](UDPPacket const& in) {
    TunnelPacket out;
    out.size = in.size;
    memcpy(out.buffer, in.data, in.size);
    return out;
  };

  loop.run(0);

  return 0;
}
