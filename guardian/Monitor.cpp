#include "guardian/Monitor.h"

#include <common/Logger.h>
#include <common/Util.h>

#include <chrono>

namespace guardian {

using namespace std::chrono_literals;

static const event::Duration kMonitorFrameDelay = 200ms;

Monitor::Monitor()
    : capture_(0), statsFrames_(new stats::RateStat("", "frames")) {
  assertTrue(capture_.isOpened(), "Cannot open camera capture.");

  double fps = capture_.get(CV_CAP_PROP_FPS);
  LOG_I("Guardian") << "FPS of the camera is: " << fps << std::endl;

  timer_.reset(new event::Timer(0s));
  processor_.reset(new event::Action({timer_->didFire()}));
  processor_->callback.setMethod<Monitor, &Monitor::doProcess>(this);
}

void Monitor::doProcess() {
  bool success = capture_.read(frame_);
  assertTrue(success, "Reading from camera failed.");

  statsFrames_->accumulate(1);
  timer_->extend(kMonitorFrameDelay);
}
}
