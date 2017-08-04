#include "networking/Tunnel.h"

#include <common/Util.h>
#include <event/IOCondition.h>

#if OSX

#include <fcntl.h>
#include <net/if_utun.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/kern_control.h>
#include <sys/kern_event.h>
#include <sys/socket.h>
#include <unistd.h>

namespace networking {

Tunnel::Tunnel() {
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

  ret = fcntl(fd, F_SETFL, O_NONBLOCK);
  checkUnixError(ret, "executing fnctl(..., O_NONBLOCK, ...)");

  fd_ = common::FileDescriptor{fd};
  deviceName = devName;
  LOG_T("Tunnel") << "Opened successfully as " << deviceName << std::endl;
}

event::Condition* Tunnel::canRead() const {
  return event::IOConditionManager::canRead(fd_.fd);
}

event::Condition* Tunnel::canWrite() const {
  return event::IOConditionManager::canWrite(fd_.fd);
}

bool Tunnel::read(TunnelPacket& packet) {
  int ret = ::read(fd_.fd, packet.data, packet.capacity);

  if (ret == 0) {
    throw TunnelClosedException("Tunnel is closed while reading.");
  }

  if (!checkRetryableError(ret, "reading a Tunnel packet.")) {
    return false;
  }

  assertTrue(ret < packet.capacity, "Tunnel packet read buffer is too small.");
  packet.size = ret;

  // OSX tunnel has header 0x00 0x00 0x00 0x02, whereas Linux tunnel has header
  // 0x00 0x00 0x08 0x00. Here we do the translation.
  assertTrue(packet.data[0] == 0x00, "header[0] != 0x00");
  assertTrue(packet.data[1] == 0x00, "header[1] != 0x00");
  assertTrue(packet.data[2] == 0x00, "header[2] != 0x00");
  assertTrue(packet.data[3] == 0x02, "header[3] != 0x02");
  packet.data[2] = 0x08;
  packet.data[3] = 0x00;

  return true;
}

bool Tunnel::write(TunnelPacket packet) {
  // OSX tunnel has header 0x00 0x00 0x00 0x02, whereas Linux tunnel has header
  // 0x00 0x00 0x08 0x00. Here we do the translation.
  assertTrue(packet.data[0] == 0x00, "header[0] != 0x00");
  assertTrue(packet.data[1] == 0x00, "header[1] != 0x00");
  assertTrue(packet.data[2] == 0x08, "header[2] != 0x08");
  assertTrue(packet.data[3] == 0x00, "header[3] != 0x00");
  packet.data[2] = 0x00;
  packet.data[3] = 0x02;

  int ret = ::write(fd_.fd, packet.data, packet.size);

  if (ret == 0) {
    throw TunnelClosedException("Tunnel is closed while sending.");
  }

  if (!checkRetryableError(ret, "writing a Tunnel packet.")) {
    return false;
  }

  assertTrue(ret == packet.size, "Tunnel packet not fully sent.");
  return true;
}
}

#endif
