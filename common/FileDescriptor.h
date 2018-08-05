#pragma once

#include <common/Util.h>

#include <unistd.h>

#include <stdexcept>
#include <utility>

namespace common {

struct FileDescriptor {
public:
  class ClosedException : public std::runtime_error {
  public:
    ClosedException(std::string const& reason) : std::runtime_error(reason) {}
  };

  FileDescriptor() : fd(-1) {}

  FileDescriptor(int fd) : fd(fd) {}

  ~FileDescriptor() {
    if (fd >= 0) {
      close(fd);
    }
  }

  FileDescriptor(FileDescriptor&& move) {
    fd = move.fd;
    move.fd = -1;
  }

  FileDescriptor& operator=(FileDescriptor&& move) {
    std::swap(fd, move.fd);
    return *this;
  }

  int fd;

  size_t atomicRead(Byte* buffer, size_t capacity) {
    int ret = ::read(fd, buffer, capacity);

    if (ret == 0) {
      throw ClosedException("File descriptor is closed while reading.");
    }

    if (!checkRetryableError(ret, "doing a file descriptor atomic read")) {
      return 0;
    }

    assertTrue(ret < capacity, "Tunnel packet read buffer is too small.");
    return ret;
  }

  bool atomicWrite(Byte* buffer, size_t size) {
    int ret = ::write(fd, buffer, size);

    if (ret == 0) {
      throw ClosedException("Tunnel is closed while sending.");
    }

    if (!checkRetryableError(ret, "doing a file descriptor atomic write")) {
      return false;
    }

    assertTrue(ret == size, "File descriptor atomic write fragmented.");
    return true;
  }

private:
  FileDescriptor(FileDescriptor const& copy) = delete;
  FileDescriptor& operator=(FileDescriptor const& copy) = delete;
};
} // namespace common
