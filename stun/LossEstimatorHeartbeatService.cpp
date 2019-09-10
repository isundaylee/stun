#include "stun/LossEstimatorHeartbeatService.h"

#include <json/json.hpp>

namespace {
using json = nlohmann::json;
};

namespace stun {

networking::Messenger::HeartbeatService
buildLossEstimatorHeartbeatService(stun::Dispatcher const& dispatcher) {
  auto name = std::string{"loss_estimator"};

  auto producer = [&dispatcher]() {
    return json{{"tx_packets", dispatcher.getStatTxPackets().getCount()},
                {"rx_packets", dispatcher.getStatRxPackets().getCount()}};
  };

  auto consumer = [&dispatcher](json const& value) {
    auto peerTxPackets = value["tx_packets"].get<size_t>();
    auto peerRxPackets = value["rx_packets"].get<size_t>();

    auto myTxPackets = dispatcher.getStatTxPackets().getCount();
    auto myRxPackets = dispatcher.getStatRxPackets().getCount();

    auto myTxLossRate =
        (myTxPackets == 0
             ? std::string{"NaN"}
             : std::to_string(1.0 - static_cast<double>(peerRxPackets) /
                                        static_cast<double>(myTxPackets)));
    auto myRxLossRate =
        (peerTxPackets == 0
             ? std::string{"NaN"}
             : std::to_string(1.0 - static_cast<double>(myRxPackets) /
                                        static_cast<double>(peerTxPackets)));

    LOG_V("LossEstimator") << "TX loss rate: " << myTxLossRate << ", "
                           << "RX loss rate: " << myRxLossRate << std::endl;
  };

  return {name, producer, consumer};
}

}; // namespace stun