#include <Tunnel.h>
#include <Util.h>
#include <UDPServer.h>
#include <UDPConnection.h>
#include <NetlinkClient.h>

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

  // while (true) {
  //   TunnelPacket packet = tunnel.readPacket();
  //   LOG() << "Read a packet with size " << packet.size << std::endl;
  //
  //   sleep(1);
  // }

  loop.run(0);
  
  return 0;
}
