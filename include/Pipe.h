#pragma once

#include <FIFO.h>
#include <Util.h>

#include <ev/ev++.h>

#include <stdexcept>

namespace stun {

const ev_tstamp kPipeStatsInterval = 1.0;
const size_t kBytesPerKiloBit = 1024 / 8;
const size_t kPipePacketBufferSize = 4096;

struct PipePacket {
  char* buffer;
  int size;

  PipePacket() {
    size = 0;
    buffer = new char[kPipePacketBufferSize];
  }

  PipePacket(PipePacket const& copy) {
    size = copy.size;
    buffer = new char[kPipePacketBufferSize];
    std::copy(copy.buffer, copy.buffer + copy.size, buffer);
  }

  PipePacket& operator=(PipePacket copy) {
    swap(copy, *this);
    return *this;
  }

  PipePacket(PipePacket&& other) {
    using std::swap;
    swap(buffer, other.buffer);
    swap(size, other.size);
  }

  ~PipePacket() {
    free(buffer);
  }

  friend void swap(PipePacket& lhs, PipePacket& rhs) noexcept {
    using std::swap;
    swap(lhs.buffer, rhs.buffer);
    swap(lhs.size, rhs.size);
  }
};

template <typename P>
class Pipe {
public:
  FIFO<P> inboundQ;
  FIFO<P> outboundQ;

  Pipe(int inboundQueueSize, int outboundQueueSize) :
      inboundQ(inboundQueueSize),
      outboundQ(outboundQueueSize),
      inboundBytes(0),
      outboundBytes(0) {}

protected:
  void setFd(int fd) {
    fd_ = fd;

    inboundWatcher_.set<Pipe, &Pipe::doReceive>(this);
    inboundWatcher_.set(fd_, EV_READ);
    inboundWatcher_.start();

    outboundWatcher_.set<Pipe, &Pipe::doSend>(this);
    outboundWatcher_.set(fd_, EV_WRITE);
    outboundWatcher_.start();

    if (shouldOutputStats) {
      statsWatcher_.set<Pipe, &Pipe::doStats>(this);
      statsWatcher_.set(0.0, kPipeStatsInterval);
      statsWatcher_.start();
    }
  }

  virtual bool write(P const& packet) = 0;
  virtual bool read(P& packet) = 0;

  std::string name = "random pipe";
  bool shouldOutputStats = false;

  int fd_;

private:
  Pipe(Pipe const&) = delete;
  Pipe& operator=(Pipe const&) = delete;

  void doReceive(ev::io& watcher, int events) {
    if (events & EV_ERROR) {
      throwUnixError("doReceive()");
    }

    if (inboundQ.full()) {
      return;
    }

    P packet;
    if (read(packet)) {
      inboundBytes += packet.size;
      inboundQ.push(packet);
    }
  }

  void doSend(ev::io& watcher, int events) {
    if (events & EV_ERROR) {
      throwUnixError("doSend()");
    }

    while (!outboundQ.empty()) {
      P packet = outboundQ.front();
      if (write(packet)) {
        outboundBytes += packet.size;
        outboundQ.pop();
      };
    }
  }

  void doStats(ev::timer& watcher, int events) {
    if (events & EV_ERROR) {
      throwUnixError("doStats()");
    }

    LOG() << name << "'s transfer rate: TX = " << (outboundBytes / kBytesPerKiloBit)
        << " Kbps, RX = " << (inboundBytes / kBytesPerKiloBit) << " Kbps" << std::endl;
    inboundBytes = 0;
    outboundBytes = 0;
  }

  ev::io inboundWatcher_;
  ev::io outboundWatcher_;
  ev::timer statsWatcher_;

  size_t inboundBytes;
  size_t outboundBytes;
};

}
