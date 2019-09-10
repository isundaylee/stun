#pragma once

#include <networking/Messenger.h>
#include <stun/Dispatcher.h>

namespace stun {

networking::Messenger::HeartbeatService
buildLossEstimatorHeartbeatService(stun::Dispatcher const& dispatcher);

}; // namespace stun
