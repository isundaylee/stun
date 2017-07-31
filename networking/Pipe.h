#pragma once

#include <common/Util.h>

#include <event/Action.h>
#include <event/Callback.h>
#include <event/FIFO.h>
#include <event/IOCondition.h>

#include <stats/RateStat.h>

#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include <memory>
#include <stdexcept>

namespace networking {

template <typename P> class Pipe {
public:
  std::unique_ptr<event::FIFO<P>> inboundQ;
  std::unique_ptr<event::FIFO<P>> outboundQ;

  bool shouldOutputStats = false;

  Pipe(int inboundQueueSize, int outboundQueueSize)
      : inboundQ(new event::FIFO<P>(inboundQueueSize)),
        outboundQ(new event::FIFO<P>(outboundQueueSize)),
        statTxPackets_(new stats::RateStat<size_t>("tx_pkts", 0)),
        statTxBytes_(new stats::RateStat<size_t>("tx_bytes", 0)),
        statRxPackets_(new stats::RateStat<size_t>("rx_pkts", 0)),
        statRxBytes_(new stats::RateStat<size_t>("rx_bytes", 0)),
        didClose_(new event::BaseCondition()) {}

  Pipe(Pipe&& move)
      : inboundQ(std::move(move.inboundQ)),
        outboundQ(std::move(move.outboundQ)),
        shouldOutputStats(move.shouldOutputStats), name_(std::move(move.name_)),
        statTxPackets_(std::move(move.statTxPackets_)),
        statTxBytes_(std::move(move.statTxBytes_)),
        statRxPackets_(std::move(move.statRxPackets_)),
        statRxBytes_(std::move(move.statRxBytes_)),
        didClose_(std::move(move.didClose_)) {
    fd_ = move.fd_;
    move.fd_ = 0;

    sender = std::move(move.sender);
    receiver = std::move(move.receiver);

    if (!!sender) {
      sender->callback.target = this;
      receiver->callback.target = this;
    }
  }

  ~Pipe() { close(); }

  virtual void open() = 0;
  void setName(std::string const& name) {
    statTxPackets_->setName(name);
    statTxBytes_->setName(name);
    statRxPackets_->setName(name);
    statRxBytes_->setName(name);
    name_ = name;
  }

  event::Condition* didClose() { return didClose_.get(); }

protected:
  void startActions() {
    assertTrue(fd_ != 0, "startActions() called when fd_ is 0");

    receiver.reset(new event::Action(
        {event::IOConditionManager::canRead(fd_), inboundQ->canPush()}));
    receiver->callback.setMethod<Pipe, &Pipe::doReceive>(this);

    sender.reset(new event::Action(
        {event::IOConditionManager::canWrite(fd_), outboundQ->canPop()}));
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
    didClose_->fire();
  }

  int fd_ = 0;
  std::string name_ = "UNNAMED-BAD";

private:
  Pipe(Pipe const&) = delete;
  Pipe& operator=(Pipe const&) = delete;

  void doReceive() {
    P packet;
    if (read(packet)) {
      statRxPackets_->accumulate(1);
      statRxBytes_->accumulate(packet.size);
      inboundQ->push(std::move(packet));
    }
  }

  void doSend() {
    while (outboundQ->canPop()->eval()) {
      P const& packet = outboundQ->front();
      if (write(packet)) {
        statTxPackets_->accumulate(1);
        statTxBytes_->accumulate(packet.size);
        outboundQ->pop();
      };
    }
  }

  std::unique_ptr<stats::RateStat<size_t>> statTxPackets_;
  std::unique_ptr<stats::RateStat<size_t>> statTxBytes_;
  std::unique_ptr<stats::RateStat<size_t>> statRxPackets_;
  std::unique_ptr<stats::RateStat<size_t>> statRxBytes_;

  std::unique_ptr<event::Action> receiver;
  std::unique_ptr<event::Action> sender;

  std::unique_ptr<event::BaseCondition> didClose_;
};
}
