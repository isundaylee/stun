#include "networking/Packet.h"

namespace networking {

/* static */ common::MemoryPool<kPacketPoolBlockSize, kPacketPoolBlockCount>
    Packet::pool_;

Packet::Packet(size_t capacity) : capacity(capacity), size(0) {
  data = static_cast<Byte*>(pool_.allocate(capacity));
}

Packet::Packet(Packet&& move) : capacity(move.capacity), size(move.size) {
  data = move.data;
  move.data = nullptr;
}

Packet::~Packet() {
  if (data != nullptr) {
    pool_.free(data);
  }
}

void Packet::fill(Byte* buffer, size_t size) {
  this->size = size;
  memcpy(data, buffer, size);
}

void Packet::fill(Packet packet) {
  std::swap(this->size, packet.size);
  std::swap(this->capacity, packet.capacity);
  std::swap(this->data, packet.data);
}
}
