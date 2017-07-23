#pragma once

#include <FIFO.h>
#include <Util.h>

#include <ev/ev++.h>
#include <unistd.h>

#include <memory>
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

  std::string name = "UNNAMED-BAD";
  bool shouldOutputStats = false;

  Pipe(int inboundQueueSize, int outboundQueueSize) :
      inboundQ(inboundQueueSize),
      outboundQ(outboundQueueSize),
      inboundBytes(0),
      outboundBytes(0),
      inboundWatcher_(new ev::io()),
      outboundWatcher_(new ev::io()),
      statsWatcher_(new ev::timer()) {}

  Pipe(Pipe&& move) :
      inboundQ(std::move(move.inboundQ)),
      outboundQ(std::move(move.outboundQ)),
      name(std::move(move.name)),
      shouldOutputStats(move.shouldOutputStats),
      inboundBytes(move.inboundBytes),
      outboundBytes(move.outboundBytes) {
    fd_ = move.fd_;
    move.fd_ = 0;

    onClose = std::move(move.onClose);
    move.onClose = []() {};
    inboundWatcher_ = std::move(move.inboundWatcher_);
    outboundWatcher_ = std::move(move.outboundWatcher_);
    statsWatcher_ = std::move(move.statsWatcher_);

    this->startWatchers();
  }

  ~Pipe() {
    close();
  }

  std::function<void (void)> onClose = []() {};

  virtual void open() = 0;

protected:
  void startWatchers() {
    assertTrue(fd_ != 0, "startWatchers() called when fd_ is 0");

    inboundWatcher_->set<Pipe, &Pipe::doReceive>(this);
    inboundWatcher_->set(fd_, EV_READ);
    inboundWatcher_->start();

    outboundWatcher_->set<Pipe, &Pipe::doSend>(this);
    outboundWatcher_->set(fd_, EV_WRITE);

    outboundQ.onBecomeNonEmpty = [this]() {
      outboundWatcher_->start();
    };

    statsWatcher_->set<Pipe, &Pipe::doStats>(this);
    statsWatcher_->set(0.0, kPipeStatsInterval);
    statsWatcher_->start();
  }

  void stopWatchers() {
    inboundWatcher_->stop();
    outboundWatcher_->stop();
    statsWatcher_->stop();

    outboundQ.onBecomeNonEmpty = []() {};
  }

  virtual bool write(P const& packet) = 0;
  virtual bool read(P& packet) = 0;

  virtual void close() {
    if (!!inboundWatcher_) {
      stopWatchers();
    }

    if (fd_ != 0) {
      ::close(fd_);
      fd_ = 0;
      onClose();
    }
  }

  int fd_ = 0;

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

    if (outboundQ.empty()) {
      outboundWatcher_->stop();
      return;
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

    if (shouldOutputStats) {
      LOG() << name << "'s transfer rate: TX = " << (outboundBytes / kBytesPerKiloBit)
          << " Kbps, RX = " << (inboundBytes / kBytesPerKiloBit) << " Kbps" << std::endl;
    }

    inboundBytes = 0;
    outboundBytes = 0;
  }

  std::unique_ptr<ev::io> inboundWatcher_;
  std::unique_ptr<ev::io> outboundWatcher_;
  std::unique_ptr<ev::timer> statsWatcher_;

  size_t inboundBytes;
  size_t outboundBytes;
};

}
