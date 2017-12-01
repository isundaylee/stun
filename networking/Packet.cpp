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

Packet& Packet::operator=(Packet&& move) {
  std::swap(capacity, move.capacity);
  std::swap(size, move.size);
  std::swap(data, move.data);

  return *this;
}

Packet::~Packet() {
  if (data != nullptr) {
    pool_.free(data);
  }
}

void Packet::fill(Byte* buffer, size_t size) {
  assertTrue(
      size <= capacity,
      "Over-capacity packet encountered, size = " + std::to_string(size) +
          ", capacity = " + std::to_string(capacity));
  this->size = size;
  memcpy(data, buffer, size);
}

void Packet::fill(Packet packet) { *this = std::move(packet); }

void Packet::trimFront(size_t bytes) {
  assertTrue(bytes <= this->size,
             "Trying to trim more bytes than there are in the packet.");

  this->size -= bytes;
  this->data += bytes;
}

void Packet::insertFront(size_t bytes) {
  Byte* newData = this->data;

  if (this->size + bytes > this->capacity) {
    this->capacity = this->size + bytes;
    newData = (Byte*)pool_.allocate(this->capacity);
  }

  memmove(newData + bytes, this->data, this->size);
  this->size += bytes;

  if (newData != this->data) {
    pool_.free(this->data);
    this->data = newData;
  }
}
}
