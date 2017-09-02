#include <common/Util.h>

#if OSX || LINUX

#include "networking/Tunnel.h"

#include <event/IOCondition.h>

#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#if OSX
#include <net/if_utun.h>
#include <sys/kern_control.h>
#include <sys/kern_event.h>
#elif LINUX
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#endif

namespace networking {

Tunnel::Tunnel() {
#if OSX
  int fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
  checkUnixError(fd, "opening utun");

  struct ctl_info info;
  memset(&info, 0, sizeof(info));
  strncpy(info.ctl_name, UTUN_CONTROL_NAME, MAX_KCTL_NAME);
  int ret = ioctl(fd, CTLIOCGINFO, &info);
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
#elif LINUX
  int fd = ::open("/dev/net/tun", O_RDWR);
  checkUnixError(fd, "opening /dev/net/tun");

  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN;
  int ret = ioctl(fd, TUNSETIFF, (void*)&ifr);
  checkUnixError(ret, "doing TUNSETIFF");

  deviceName = ifr.ifr_ifrn.ifrn_name;
#elif IOS
  LOG_E("Tunnel") << "Tunnel is not yet supported on iOS" << std::endl;
  throw std::runtime_error("Tunnel is not yet supported on iOS");
#endif

#if LINUX || OSX
  ret = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK, 0);
  checkUnixError(ret, "setting O_NONBLOCK for Tunnel");

  fd_ = common::FileDescriptor{fd};
  LOG_V("Tunnel") << "Opened successfully as " << deviceName << std::endl;
#endif
}

event::Condition* Tunnel::canRead() const {
  return event::IOConditionManager::canRead(fd_.fd);
}

event::Condition* Tunnel::canWrite() const {
  return event::IOConditionManager::canWrite(fd_.fd);
}

bool Tunnel::read(TunnelPacket& packet) {
  size_t read = fd_.atomicRead(packet.data, packet.capacity);
  if (read == 0) {
    return false;
  } else {
    packet.size = read;
  }

#if OSX
  // OSX tunnel has header 0x00 0x00 0x00 0x02, whereas Linux tunnel has header
  // 0x00 0x00 0x08 0x00. Here we do the translation.
  assertTrue(packet.data[0] == 0x00, "header[0] != 0x00");
  assertTrue(packet.data[1] == 0x00, "header[1] != 0x00");
  assertTrue(packet.data[2] == 0x00, "header[2] != 0x00");
  assertTrue(packet.data[3] == 0x02, "header[3] != 0x02");
  packet.data[2] = 0x08;
  packet.data[3] = 0x00;
#endif

  return true;
}

bool Tunnel::write(TunnelPacket packet) {
#if OSX
  packet.data[0] = 0x00;
  packet.data[1] = 0x00;
  packet.data[2] = 0x00;
  packet.data[3] = 0x02;
#elif LINUX
  packet.data[0] = 0x00;
  packet.data[1] = 0x00;
  packet.data[2] = 0x08;
  packet.data[3] = 0x00;
#else
#error "Unexpected OS"
#endif

  return fd_.atomicWrite(packet.data, packet.size);
}
}

#endif
