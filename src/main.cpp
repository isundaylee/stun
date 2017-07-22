#include <Tunnel.h>
#include <Util.h>
#include <NetlinkClient.h>

#include <unistd.h>

#include <iostream>

using namespace stun;

int main(int argc, char* argv[]) {
  Tunnel tunnel(TunnelType::TUN);
  NetlinkClient netlink;

  netlink.newLink(tunnel.getDeviceName());
  netlink.setLinkAddress(tunnel.getDeviceName(), "10.100.0.1", "10.100.0.2");

  while (true) {
    TunnelPacket packet = tunnel.readPacket();
    LOG() << "Read a packet with size " << packet.size << std::endl;

    sleep(1);
  }

  return 0;
}
