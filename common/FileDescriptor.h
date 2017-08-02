#pragma once

#include <unistd.h>

#include <utility>

namespace common {

struct FileDescriptor {
public:
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

private:
  FileDescriptor(FileDescriptor const& copy) = delete;
  FileDescriptor& operator=(FileDescriptor const& copy) = delete;
};
}
