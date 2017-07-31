#include "networking/Tunnel.h"

#include <common/Util.h>

// Ugly hack to make clang-format happy, as this has to precede <linux/if.h>
// below -- oh well, linux header files ):
#include <sys/socket.h>

#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <stdexcept>

namespace networking {

const size_t kTunnelInboundQueueSize = 256;
const size_t kTunnelOutboundQueueSize = 256;

Tunnel::Tunnel(TunnelType type)
    : Pipe(kTunnelInboundQueueSize, kTunnelOutboundQueueSize), type_(type) {}

void Tunnel::open() {
  assertTrue(this->fd_ == 0, "trying to open an already open Tunnel");

  int ret = ::open("/dev/net/tun", O_RDWR);
  checkUnixError(ret, "opening /dev/net/tun");
  fd_ = ret;

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = (type_ == TUN ? IFF_TUN : IFF_TAP);
  ret = ioctl(fd_, TUNSETIFF, (void*)&ifr);
  checkUnixError(ret, "doing TUNSETIFF");

  ret = fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK, 0);
  checkUnixError(ret, "setting O_NONBLOCK for Tunnel");

  devName_ = ifr.ifr_name;
  LOG_T(name_) << "Opened successfully as " << devName_ << std::endl;

  this->fd_ = fd_;
  this->startActions();
}

bool Tunnel::read(TunnelPacket& packet) {
  int ret = ::read(fd_, packet.data, kTunnelBufferSize);
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
  int ret = ::write(fd_, packet.data, packet.size);
  if (!checkRetryableError(ret, "writing a tunnel packet")) {
    return false;
  }
  assertTrue(ret == packet.size, "Packet fragmented");
  return true;
}
}
