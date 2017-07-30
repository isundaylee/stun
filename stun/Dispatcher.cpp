#include "stun/Dispatcher.h"

namespace stun {

Dispatcher::Dispatcher(networking::Tunnel&& tunnel)
    : tunnel_(std::move(tunnel)) {}

void Dispatcher::start() {
  sender_.reset(new event::Action({tunnel_.inboundQ->canPop(),
                                   dataPipe_->outboundQ->canPush(),
                                   dataPipe_->isPrimed()}));
  sender_->callback.setMethod<Dispatcher, &Dispatcher::doSend>(this);

  receiver_.reset(new event::Action(
      {dataPipe_->inboundQ->canPop(), tunnel_.outboundQ->canPush()}));
  receiver_->callback.setMethod<Dispatcher, &Dispatcher::doReceive>(this);

  dataPipe_->start();
}

void Dispatcher::doSend() {
  while (tunnel_.inboundQ->canPop()->eval() &&
         dataPipe_->outboundQ->canPush()->eval()) {
    TunnelPacket in = tunnel_.inboundQ->pop();
    DataPacket out;
    out.fill(in.data, in.size);
    dataPipe_->outboundQ->push(out);
  }
}

void Dispatcher::doReceive() {
  while (dataPipe_->inboundQ->canPop()->eval() &&
         tunnel_.outboundQ->canPush()->eval()) {
    DataPacket in = dataPipe_->inboundQ->pop();
    TunnelPacket out;
    out.fill(in.data, in.size);
    tunnel_.outboundQ->push(out);
  }
}

void Dispatcher::addDataPipe(DataPipe* dataPipe) { dataPipe_.reset(dataPipe); }
}
