#include "guardian/Monitor.h"

#include <common/Logger.h>
#include <common/Util.h>

#include <opencv2/imgproc/imgproc.hpp>

#include <chrono>

namespace guardian {

using namespace std::chrono_literals;

static const event::Duration kMonitorFrameDelay = 25ms;
static const double kMonitorBackgroundAlpha = 0.10;
static const double kMonitorThreshold = 25.0;
static const int kMonitorGaussianRadius = 15;
static const double kMonitorGaussianSigma = 100.0;

Monitor::Monitor()
    : capture_(0), statsFrames_(new stats::RateStat("", "frames")),
      statsMotionLevel_(new stats::RateStat("", "motion_level")) {
  assertTrue(capture_.isOpened(), "Cannot open camera capture.");

  auto fps = capture_.get(CV_CAP_PROP_FPS);
  LOG_I("Guardian") << "FPS of the camera is: " << fps << std::endl;

  timer_.reset(new event::Timer(0s));
  processor_.reset(new event::Action({timer_->didFire()}));
  processor_->callback.setMethod<Monitor, &Monitor::doProcess>(this);
}

void Monitor::doProcess() {
  statsFrames_->accumulate(1);
  timer_->extend(kMonitorFrameDelay);

  // Capture a new frame
  auto colorFrame = cv::Mat{};
  auto success = capture_.read(colorFrame);
  assertTrue(success, "Reading from camera failed.");

  // Turns it into grayscale
  auto frame = cv::Mat{};
  cv::cvtColor(colorFrame, frame, CV_RGB2GRAY);

  // Blur it
  cv::GaussianBlur(frame, frame,
                   cv::Size(kMonitorGaussianRadius, kMonitorGaussianRadius),
                   kMonitorGaussianSigma);

  // Calculates running average for background
  if (background_.empty()) {
    frame.assignTo(background_, CV_32FC1);
  } else {
    cv::accumulateWeighted(frame, background_, kMonitorBackgroundAlpha);
  }

  // Converts background back to int type
  auto intBackground = cv::Mat(background_.size(), frame.type());
  cv::convertScaleAbs(background_, intBackground, 1.0);

  // Calculates diff -- this is the motion
  auto diff = cv::Mat{};
  cv::absdiff(frame, intBackground, diff);
  cv::threshold(diff, diff, kMonitorThreshold, 255, CV_THRESH_BINARY);

  auto motionCount = 0;
  for (auto i = 0; i < diff.rows; i++) {
    for (auto j = 0; j < diff.cols; j++) {
      if (diff.at<Byte>(i, j) > 0) {
        motionCount++;
      }
    }
  }

  statsMotionLevel_->accumulate(1.0 * motionCount / (diff.rows * diff.cols) *
                                (1.0 * kMonitorFrameDelay / 1s));

  return;
}
}
