#pragma once

#include <event/Condition.h>
#include <event/FIFO.h>

#include <networking/Packet.h>
#include <networking/UDPSocket.h>

namespace {
static const size_t kDataPacketSize = 2048;
};

namespace stun {

class DataPacket : public networking::Packet {
public:
  DataPacket() : Packet(kDataPacketSize) {}
};

class CoreDataPipe {
public:
  CoreDataPipe() {}

  virtual ~CoreDataPipe() = default;

  virtual event::Condition* canSend() = 0;
  virtual event::Condition* canReceive() = 0;

  virtual bool send(DataPacket packet) = 0;
  virtual bool receive(DataPacket& output) = 0;
};

}; // namespace stun