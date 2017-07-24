#pragma once

#include <common/Util.h>

#include <event/Action.h>
#include <event/Callback.h>
#include <event/FIFO.h>
#include <event/IOCondition.h>

#include <unistd.h>

#include <memory>
#include <stdexcept>

namespace networking {

const double kPipeStatsInterval = 1.0;
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
  std::unique_ptr<event::FIFO<P>> inboundQ;
  std::unique_ptr<event::FIFO<P>> outboundQ;

  std::string name = "UNNAMED-BAD";
  bool shouldOutputStats = false;

  Pipe(int inboundQueueSize, int outboundQueueSize) :
      inboundQ(new event::FIFO<P>(inboundQueueSize)),
      outboundQ(new event::FIFO<P>(outboundQueueSize)),
      inboundBytes(0),
      outboundBytes(0) {}

  Pipe(Pipe&& move) :
      inboundQ(std::move(move.inboundQ)),
      outboundQ(std::move(move.outboundQ)),
      name(std::move(move.name)),
      shouldOutputStats(move.shouldOutputStats),
      inboundBytes(move.inboundBytes),
      outboundBytes(move.outboundBytes) {
    fd_ = move.fd_;
    move.fd_ = 0;

    sender = std::move(move.sender);
    receiver = std::move(move.receiver);
    onClose = std::move(move.onClose);

    if (!!sender) {
      sender->callback.target = this;
      receiver->callback.target = this;
    }

    onClose.target = this;
  }

  ~Pipe() {
    close();
  }


  virtual void open() = 0;

  event::Callback onClose;

protected:
  void startActions() {
    assertTrue(fd_ != 0, "startActions() called when fd_ is 0");

    receiver.reset(new event::Action({event::IOConditionManager::canRead(fd_), inboundQ->canPush()}));
    receiver->callback.setMethod<Pipe, &Pipe::doReceive>(this);

    sender.reset(new event::Action({event::IOConditionManager::canWrite(fd_), outboundQ->canPop()}));
    sender->callback.setMethod<Pipe, &Pipe::doSend>(this);
  }

  void stopActions() {
    receiver.reset();
    sender.reset();
  }

  virtual bool write(P const& packet) = 0;
  virtual bool read(P& packet) = 0;

  virtual void close() {
    if (fd_ == 0) {
      return;
    }

    event::IOConditionManager::close(fd_);
    ::close(fd_);
    fd_ = 0;
    onClose.invoke();
  }

  int fd_ = 0;

private:
  Pipe(Pipe const&) = delete;
  Pipe& operator=(Pipe const&) = delete;

  void doReceive() {
    P packet;
    if (read(packet)) {
      inboundBytes += packet.size;
      inboundQ->push(packet);
    }
  }

  void doSend() {
    while (outboundQ->canPop()->value) {
      P packet = outboundQ->front();
      if (write(packet)) {
        outboundBytes += packet.size;
        outboundQ->pop();
      };
    }
  }

  void doStats() {
    if (shouldOutputStats) {
      LOG() << name << "'s transfer rate: TX = " << (outboundBytes / kBytesPerKiloBit)
          << " Kbps, RX = " << (inboundBytes / kBytesPerKiloBit) << " Kbps" << std::endl;
    }

    inboundBytes = 0;
    outboundBytes = 0;
  }

  std::unique_ptr<event::Action> receiver;
  std::unique_ptr<event::Action> sender;

  size_t inboundBytes;
  size_t outboundBytes;
};

}
