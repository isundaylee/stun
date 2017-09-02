#pragma once

#include <common/MemoryPool.h>
#include <common/Util.h>

#include <string.h>
#include <unistd.h>

#include <vector>

namespace networking {

const static size_t kPacketPoolBlockSize = 4096;
const static size_t kPacketPoolBlockCount = 32;

struct Packet {
  size_t capacity;
  size_t size;
  Byte* data;

  Packet(size_t capacity);
  Packet(Packet&& move);
  Packet& operator=(Packet&& move);
  ~Packet();

  void fill(Byte* buffer, size_t size);
  void fill(Packet packet);

  template <typename T> void pack(T const& obj) {
    fill((Byte*)&obj, sizeof(obj));
  }

  template <typename T> T unpack() {
    T obj;
    memcpy(&obj, data, sizeof(obj));
    return obj;
  }

private:
  Packet(Packet const& copy) = delete;
  Packet& operator=(Packet const& copy) = delete;

  static common::MemoryPool<kPacketPoolBlockSize, kPacketPoolBlockCount> pool_;
};
}
