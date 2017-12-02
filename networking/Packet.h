#pragma once

#include <common/MemoryPool.h>
#include <common/Util.h>

#include <string.h>
#include <unistd.h>

#include <array>
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

  void trimFront(size_t bytes);
  void insertFront(size_t bytes);

  template <size_t A, size_t B>
  void replaceHeader(std::array<Byte, A> const& oldHeader,
                     std::array<Byte, B> const& newHeader) {
    for (size_t i = 0; i < oldHeader.size(); i++) {
      assertTrue(this->data[i] == oldHeader[i],
                 "header[" + std::to_string(i) +
                     "] != " + std::to_string(oldHeader[i]));
    }

    if (newHeader.size() > oldHeader.size()) {
      this->insertFront(newHeader.size() - oldHeader.size());
    }

    if (newHeader.size() < oldHeader.size()) {
      this->trimFront(oldHeader.size() - newHeader.size());
    }

    for (size_t i = 0; i < newHeader.size(); i++) {
      this->data[i] = newHeader[i];
    }
  }

private:
  Packet(Packet const& copy) = delete;
  Packet& operator=(Packet const& copy) = delete;

  static common::MemoryPool<kPacketPoolBlockSize, kPacketPoolBlockCount> pool_;
};
}
