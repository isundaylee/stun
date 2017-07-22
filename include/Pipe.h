#pragma once

#include <FIFO.h>
#include <Util.h>

#include <ev/ev++.h>

#include <stdexcept>

namespace stun {

template <typename P>
class Pipe {
public:
  FIFO<P> inboundQ;
  FIFO<P> outboundQ;

  Pipe(int inboundQueueSize, int outboundQueueSize) :
      inboundQ(inboundQueueSize),
      outboundQ(outboundQueueSize) {}

protected:
  void setFd(int fd) {
    fd_ = fd;

    inboundWatcher_.set<Pipe, &Pipe::doReceive>(this);
    inboundWatcher_.set(fd_, EV_READ);
    inboundWatcher_.start();

    outboundWatcher_.set<Pipe, &Pipe::doSend>(this);
    outboundWatcher_.set(fd_, EV_WRITE);
    outboundWatcher_.start();
  }

  virtual bool write(P const& packet) = 0;
  virtual bool read(P& packet) = 0;

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
      inboundQ.push(packet);
    }

    LOG() << "RECEIVING!!" << std::endl;
  }

  void doSend(ev::io& watcher, int events) {
    if (events & EV_ERROR) {
      throwUnixError("doSend()");
    }

    while (!outboundQ.empty()) {
      P packet = outboundQ.front();
      if (write(packet)) {
        outboundQ.pop();
      };
      LOG() << "SENDING!!" << std::endl;
    }
  }

  ev::io inboundWatcher_;
  ev::io outboundWatcher_;
};

}
