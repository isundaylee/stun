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

const size_t kTunnelInboundQueueSize = 32;
const size_t kTunnelOutboundQueueSize = 32;

Tunnel::Tunnel(TunnelType type) :
    inboundQ(kTunnelInboundQueueSize),
    outboundQ(kTunnelOutboundQueueSize) {
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

  if (fcntl(fd_, F_SETFL, fcntl(fd_, F_GETFL, 0) | O_NONBLOCK, 0) < 0) {
    throwUnixError("setting O_NONBLOCK for Tunnel");
  }

  devName_ = ifr.ifr_name;
  LOG() << "Successfully opened tunnel " << devName_ << std::endl;

  inboundWatcher_.set<Tunnel, &Tunnel::doReceive>(this);
  inboundWatcher_.set(fd_, EV_READ);
  inboundWatcher_.start();

  outboundWatcher_.set<Tunnel, &Tunnel::doSend>(this);
  outboundWatcher_.set(fd_, EV_WRITE);
  outboundWatcher_.start();
}

void Tunnel::doReceive(ev::io& watcher, int events) {
  if (events & EV_ERROR) {
    throwUnixError("Tunnel doReceive()");
  }

  if (inboundQ.full()) {
    return;
  }

  TunnelPacket packet;
  int ret = read(fd_, packet.buffer, kTunnelBufferSize);
  if (ret < 0) {
    if (errno != EAGAIN) {
      throwUnixError("reading a tunnel packet");
    }
    return;
  }
  if (ret == kTunnelBufferSize) {
    throw std::runtime_error("kTunnelBufferSize reached");
  }
  packet.size = ret;
  inboundQ.push(packet);
  LOG() << "Picking up " << packet.size << " bytes ==> " << devName_ << std::endl;
}

void Tunnel::doSend(ev::io& watcher, int events) {
  if (events & EV_ERROR) {
    throwUnixError("Tunnel doSend()");
  }

  if (outboundQ.empty()) {
    return;
  }

  while (!outboundQ.empty()) {
    TunnelPacket packet = outboundQ.pop();
    int ret = write(fd_, packet.buffer, packet.size);
    if (ret < 0) {
      if (errno != EAGAIN) {
        throwUnixError("writing a tunnel packet");
      }
      return;
    }
    if (ret != packet.size) {
      throw std::runtime_error("Packet fragmented");
    }
    LOG() << "Delivering " << devName_ << " ==> " << packet.size << " bytes" << std::endl;
  }
}

}
