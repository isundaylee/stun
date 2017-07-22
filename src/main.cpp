#include <Tunnel.h>
#include <Util.h>
#include <UDPServer.h>
#include <UDPConnection.h>
#include <NetlinkClient.h>

#include <unistd.h>

#include <iostream>

using namespace stun;

int main(int argc, char* argv[]) {
  Tunnel tunnel(TunnelType::TUN);
  NetlinkClient netlink;

  netlink.newLink(tunnel.getDeviceName());
  netlink.setLinkAddress(tunnel.getDeviceName(), "10.100.0.1", "10.100.0.2");

  UDPServer server(2859);
  server.bind();
  UDPPacket packet = server.receivePacket();
  LOG() << "Received a packet with size " << packet.size << std::endl;

  // while (true) {
  //   TunnelPacket packet = tunnel.readPacket();
  //   LOG() << "Read a packet with size " << packet.size << std::endl;
  //
  //   sleep(1);
  // }

  return 0;
}
