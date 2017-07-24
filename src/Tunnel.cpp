#include "Tunnel.h"

#include <common/Util.h>

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

const size_t kTunnelInboundQueueSize = 32;
const size_t kTunnelOutboundQueueSize = 32;

Tunnel::Tunnel(TunnelType type) :
    Pipe(kTunnelInboundQueueSize, kTunnelOutboundQueueSize),
    type_(type) {}

void Tunnel::open() {
  assertTrue(this->fd_ == 0, "trying to open an already open Tunnel");

  int ret = ::open("/dev/net/tun", O_RDWR);
  checkUnixError(ret, "opening /dev/net/tun");
  fd_ = ret;

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = (type_ == TUN ? IFF_TUN : IFF_TAP);
  ret = ioctl(fd_, TUNSETIFF, (void *) &ifr);
  checkUnixError(ret, "doing TUNSETIFF");

  ret = fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK, 0);
  checkUnixError(ret, "setting O_NONBLOCK for Tunnel");

  devName_ = ifr.ifr_name;
  LOG() << "Successfully opened tunnel " << devName_ << std::endl;

  name = "tunnel " + devName_;
  this->fd_ = fd_;
  this->startActions();
}

bool Tunnel::read(TunnelPacket& packet) {
  int ret = ::read(fd_, packet.buffer, kTunnelBufferSize);
  if (!checkRetryableError(ret, "reading a tunnel packet")) {
    return false;
  }
  assertTrue(ret < kTunnelBufferSize, "kTunnelBufferSize reached");
  packet.size = ret;
  if (ret == 0) {
    close();
    return false;
  }
  return true;
}

bool Tunnel::write(TunnelPacket const& packet) {
  int ret = ::write(fd_, packet.buffer, packet.size);
  if (!checkRetryableError(ret, "writing a tunnel packet")) {
    return false;
  }
  assertTrue(ret == packet.size, "Packet fragmented");
  return true;
}

}
