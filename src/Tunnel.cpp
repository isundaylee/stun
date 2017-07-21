#include "Tunnel.h"

#include <Util.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <stdexcept>

using namespace stun;

namespace stun {

Tunnel::Tunnel(TunnelType type) {
  type_ = type;

  int err;

  if ((err = open("/dev/net/tun", O_RDWR)) < 0) {
    throwUnixError("opening /dev/net/tun");
  } else {
    fd_ = err;
  }

  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = (type == TUN ? IFF_TUN : IFF_TAP);

  if ((err = ioctl(fd_, TUNSETIFF, (void *) &ifr)) < 0) {
    close(fd_);
    throwUnixError("doing TUNSETIFF");
  }

  devName_ = ifr.ifr_name;
  LOG() << "Successfully opened tunnel " << devName_ << std::endl;
}

TunnelPacket Tunnel::readPacket() {
  TunnelPacket packet;

  packet.size = read(fd_, packet.buffer, kTunnelBufferSize);

  if (packet.size < 0) {
    throwUnixError("reading from tunnel");
  }

  return packet;
}

}
