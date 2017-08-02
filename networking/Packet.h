#pragma once

#include <common/Util.h>

#include <string.h>
#include <unistd.h>

#include <vector>

namespace networking {

struct Packet {
  size_t capacity;
  size_t size;
  Byte* data;

  Packet(size_t capacity) : capacity(capacity), size(0) {
    data = new Byte[capacity];
  }

  ~Packet() {
    if (data != nullptr) {
      delete[] data;
    }
  }

  Packet(Packet&& move) : size(move.size) {
    data = move.data;
    move.data = nullptr;
  }

  void fill(Byte* buffer, size_t size) {
    this->size = size;
    memcpy(data, buffer, size);
  }

  void fill(Packet packet) {
    std::swap(this->size, packet.size);
    std::swap(this->data, packet.data);
  }

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
};
}
