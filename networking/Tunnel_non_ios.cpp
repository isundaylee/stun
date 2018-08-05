#include <common/Util.h>

#if TARGET_OSX || TARGET_LINUX || TARGET_BSD

#include "networking/Tunnel.h"

#include <event/IOCondition.h>

#include <array>

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#if TARGET_OSX
#include <net/if_utun.h>
#include <sys/kern_control.h>
#include <sys/kern_event.h>
#elif TARGET_LINUX
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#elif TARGET_BSD
#include <net/if.h>
#include <net/if_tun.h>
#include <stdio.h>
#else
#error "Unsupported platform."
#endif

namespace networking {

Tunnel::Tunnel() {
  int ret;

#if TARGET_OSX
  int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  checkUnixError(fd, "opening utun");

  struct ctl_info info;
  memset(&info, 0, sizeof(info));
  strncpy(info.ctl_name, UTUN_CONTROL_NAME, MAX_KCTL_NAME);
  ret = ioctl(fd, CTLIOCGINFO, &info);
  checkUnixError(ret, "executing ioctl(.., CTLIOCGINFO, ...)");

  struct sockaddr_ctl addr;
  addr.sc_len = sizeof(addr);
  addr.sc_family = AF_SYSTEM;
  addr.ss_sysaddr = AF_SYS_CONTROL;
  addr.sc_id = info.ctl_id;
  addr.sc_unit = 0;
  ret = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
  checkUnixError(ret, "connecting to utun");

  char devName[10];
  socklen_t devNameLen = sizeof(devName);
  ret = getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME, devName, &devNameLen);
  checkUnixError(ret, "executing getsockopt(..., UTUN_OPT_IFNAME, ...)");
  deviceName = devName;
#elif TARGET_LINUX
  int fd = ::open("/dev/net/tun", O_RDWR);
  checkUnixError(fd, "opening /dev/net/tun");

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN;
  ret = ioctl(fd, TUNSETIFF, (void*)&ifr);
  checkUnixError(ret, "doing TUNSETIFF");

  deviceName = ifr.ifr_ifrn.ifrn_name;
#elif TARGET_BSD
  int fd = ::open("/dev/tun0", O_RDWR);
  checkUnixError(fd, "opening /dev/tun0");

  deviceName = "tun0";
#elif TARGET_IOS
  LOG_E("Tunnel") << "Tunnel is not yet supported on iOS" << std::endl;
  throw std::runtime_error("Tunnel is not yet supported on iOS");
#else
#error "Unsupported platform."
#endif

#if TARGET_LINUX || TARGET_OSX || TARGET_BSD
  ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK, 0);
  checkUnixError(ret, "setting O_NONBLOCK for Tunnel");

  fd_ = common::FileDescriptor{fd};
  LOG_V("Tunnel") << "Opened successfully as " << deviceName << std::endl;
#else
#error "Unsupported platform."
#endif
}

event::Condition* Tunnel::canRead() const {
  return event::IOConditionManager::canRead(fd_.fd);
}

event::Condition* Tunnel::canWrite() const {
  return event::IOConditionManager::canWrite(fd_.fd);
}

#if TARGET_OSX
static std::array<Byte, 4> kTunnelPacketHeader = {{0x00, 0x00, 0x00, 0x02}};
#elif TARGET_LINUX
static std::array<Byte, 4> kTunnelPacketHeader = {{0x00, 0x00, 0x08, 0x00}};
#elif TARGET_BSD
static std::array<Byte, 0> kTunnelPacketHeader = {{}};
#else
#error "Unexpected platform."
#endif

// This is the packet header that should be handed out from and to Tunnel
static std::array<Byte, 4> kTunnelCanonicalPacketHeader = {
    {0x00, 0x00, 0x08, 0x00}};

bool Tunnel::read(TunnelPacket& packet) {
  size_t read = fd_.atomicRead(packet.data, packet.capacity);
  if (read == 0) {
    return false;
  } else {
    packet.size = read;
  }

  packet.replaceHeader(kTunnelPacketHeader, kTunnelCanonicalPacketHeader);

  return true;
}

bool Tunnel::write(TunnelPacket packet) {
  packet.replaceHeader(kTunnelCanonicalPacketHeader, kTunnelPacketHeader);

  return fd_.atomicWrite(packet.data, packet.size);
}
} // namespace networking

#endif
