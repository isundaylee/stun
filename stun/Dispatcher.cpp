#include "stun/Dispatcher.h"

#include <event/Trigger.h>

namespace stun {

using networking::TunnelClosedException;

Dispatcher::Dispatcher(event::EventLoop& loop,
                       std::unique_ptr<networking::Tunnel> tunnel)
    : loop_(loop), tunnel_(std::move(tunnel)),
      canSend_(loop.createComputedCondition()),
      canReceive_(loop.createComputedCondition()),
      statTxPackets_("Connection", "tx_packets"),
      statTxBytes_("Connection", "tx_bytes"),
      statRxPackets_("Connection", "rx_packets"),
      statRxBytes_("Connection", "rx_bytes"),
      statEfficiency_("Connection", "efficiency") {
  canSend_->expression.setMethod<Dispatcher, &Dispatcher::calculateCanSend>(
      this);
  canReceive_->expression
      .setMethod<Dispatcher, &Dispatcher::calculateCanReceive>(this);

  sender_ = loop.createAction("stun::Dispatcher::sender_",
                              {tunnel_->canRead(), canSend_.get()});
  sender_->callback.setMethod<Dispatcher, &Dispatcher::doSend>(this);

  receiver_ = loop.createAction("stun::Dispatcher::receiver_",
                                {canReceive_.get(), tunnel_->canWrite()});
  receiver_->callback.setMethod<Dispatcher, &Dispatcher::doReceive>(this);
}

bool Dispatcher::calculateCanSend() {
  // Here and in doSend(), we are only checking whether the queue can accept new
  // packets. However, data pipes with empty queues might not be able to push
  // out those packets yet (e.g. due to an unconnected UDP server socket).
  //
  // TODO: Should we do better checks here?
  for (auto const& dataPipe_ : dataPipes_) {
    if (dataPipe_->outboundQ->canPush()->eval()) {
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
  bool sent = false;

  // Finding a data pipe that can accept packets
  for (int i = 0; i < dataPipes_.size(); i++) {
    int pipeIndex = (currentDataPipeIndex_ + i) % dataPipes_.size();

    if (dataPipes_[pipeIndex]->outboundQ->canPush()->eval()) {
      // Found a data pipe that can accept packets
      // Push as many as possible
      while (dataPipes_[pipeIndex]->outboundQ->canPush()->eval()) {
        TunnelPacket in;

        try {
          auto ret = tunnel_->read(in);
          if (!ret) {
            break;
          }
        } catch (TunnelClosedException const& ex) {
          LOG_E("Dispatcher") << "Tunnel is closed: " << ex.what() << std::endl;
          assertTrue(false, "Tunnel should never close.");
        }

        assertTrue(
            in.data[0] == 0x00 && in.data[1] == 0x00 && in.data[2] == 0x08 &&
                in.data[3] == 0x00,
            "Outgoing packets read from tunnel need to have header 0x00 0x00 "
            "0x08 0x00. Got instead: " +
                std::to_string(in.data[0]) + " " + std::to_string(in.data[1]) +
                " " + std::to_string(in.data[2]) + " " +
                std::to_string(in.data[3]));

        DataPacket out;
        bytesDispatched += in.size;
        statTxPackets_.accumulate();
        statTxBytes_.accumulate(in.size);
        out.fill(std::move(in));

        dataPipes_[pipeIndex]->outboundQ->push(std::move(out));
      }

      sent = true;
      break;
    }
  }

  currentDataPipeIndex_ = (currentDataPipeIndex_ + 1) % dataPipes_.size();
  assertTrue(sent, "Cannot find a free DataPipe to send to.");
}

void Dispatcher::doReceive() {
  bool received = false;

  for (auto const& dataPipe_ : dataPipes_) {
    while (dataPipe_->inboundQ->canPop()->eval()) {
      TunnelPacket in;
      in.fill(dataPipe_->inboundQ->pop());
      bytesDispatched += in.size;
      statRxPackets_.accumulate();
      statRxBytes_.accumulate(in.size);

      if (!tunnel_->write(std::move(in))) {
        LOG_I("Dispatcher") << "Dropped an incoming packet." << std::endl;
        return;
      }

      received = true;
    }
  }

  assertTrue(received, "Cannot find a ready DataPipe to receive from.");
}

void Dispatcher::addDataPipe(std::unique_ptr<DataPipe> dataPipe) {
  dataPipe->statEfficiency = &statEfficiency_;
  DataPipe* pipe = dataPipe.get();
  dataPipes_.emplace_back(std::move(dataPipe));

  // Trigger to remove the DataPipe upon it closing
  loop_.arm("stun::Dispatcher::dataPipeClosedTrigger", {pipe->didClose()},
            [this, pipe]() {
              auto it = std::find_if(
                  dataPipes_.begin(), dataPipes_.end(),
                  [pipe](std::unique_ptr<DataPipe> const& currentPipe) {
                    return currentPipe.get() == pipe;
                  });

              assertTrue(it != dataPipes_.end(),
                         "Cannot find the DataPipe to remove.");
              dataPipes_.erase(it);
            });
}
} // namespace stun
