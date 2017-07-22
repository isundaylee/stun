#include <Tunnel.h>
#include <Util.h>
#include <UDPServer.h>
#include <UDPConnection.h>
#include <NetlinkClient.h>
#include <PacketTranslator.h>

#include <ev/ev++.h>
#include <unistd.h>

#include <iostream>

using namespace stun;

int main(int argc, char* argv[]) {
  ev::default_loop loop;

  Tunnel tunnel(TunnelType::TUN);
  NetlinkClient netlink;

  netlink.newLink(tunnel.getDeviceName());
  netlink.setLinkAddress(tunnel.getDeviceName(), "10.100.0.1", "10.100.0.2");

  UDPServer server(2859);
  server.onReceive = [](UDPPacket const& packet) {
    LOG() << "Received a packet with size " << packet.size << std::endl;
  };
  server.bind();

  PacketTranslator<UDPPacket> translator(server.inboundQ, server.outboundQ);
  translator.transform = [](UDPPacket const& packet) {
    UDPPacket outPacket;
    sprintf(outPacket.data, "Hello back, %c!\n", packet.data[0]);
    outPacket.size = strlen(outPacket.data);
    return outPacket;
  };

  loop.run(0);

  return 0;
}
