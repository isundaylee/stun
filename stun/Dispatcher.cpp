#include "stun/Dispatcher.h"

namespace stun {

Dispatcher::Dispatcher(networking::Tunnel&& tunnel)
    : tunnel_(std::move(tunnel)), canSend_(new event::ComputedCondition()),
      canReceive_(new event::ComputedCondition()) {
  canSend_->expression.setMethod<Dispatcher, &Dispatcher::calculateCanSend>(
      this);
  canReceive_->expression
      .setMethod<Dispatcher, &Dispatcher::calculateCanReceive>(this);
}

void Dispatcher::start() {
  sender_.reset(
      new event::Action({tunnel_.inboundQ->canPop(), canSend_.get()}));
  sender_->callback.setMethod<Dispatcher, &Dispatcher::doSend>(this);

  receiver_.reset(
      new event::Action({canReceive_.get(), tunnel_.outboundQ->canPush()}));
  receiver_->callback.setMethod<Dispatcher, &Dispatcher::doReceive>(this);
}

bool Dispatcher::calculateCanSend() {
  for (auto const& dataPipe_ : dataPipes_) {
    if (dataPipe_->isPrimed()->eval() &&
        dataPipe_->outboundQ->canPush()->eval()) {
      return true;
    }
  }
  return false;
}

bool Dispatcher::calculateCanReceive() {
  for (auto const& dataPipe_ : dataPipes_) {
    if (dataPipe_->inboundQ->canPop()->eval()) {
      return true;
    }
  }
  return false;
}

void Dispatcher::doSend() {
  while (tunnel_.inboundQ->canPop()->eval() && canSend_->eval()) {
    TunnelPacket in = tunnel_.inboundQ->pop();
    DataPacket out;
    out.fill(in.data, in.size);

    bool sent = false;
    for (auto const& dataPipe_ : dataPipes_) {
      if (dataPipe_->isPrimed()->eval() &&
          dataPipe_->outboundQ->canPush()->eval()) {
        dataPipe_->outboundQ->push(out);
        sent = true;
        break;
      }
    }

    assertTrue(sent, "Cannot find a free DataPipe to send to.");
  }
}

void Dispatcher::doReceive() {
  while (canReceive_->eval() && tunnel_.outboundQ->canPush()->eval()) {
    DataPacket in;
    bool received = false;
    for (auto const& dataPipe_ : dataPipes_) {
      if (dataPipe_->inboundQ->canPop()->eval()) {
        in = dataPipe_->inboundQ->pop();
        received = true;
        break;
      }
    }
    assertTrue(received, "Cannot find a ready DataPipe to receive from.");
    TunnelPacket out;
    out.fill(in.data, in.size);
    tunnel_.outboundQ->push(out);
  }
}

void Dispatcher::addDataPipe(DataPipe* dataPipe) {
  dataPipes_.emplace_back(dataPipe);
}
}
