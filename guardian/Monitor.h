#pragma once

#include <event/Action.h>
#include <event/Timer.h>
#include <stats/RateStat.h>

#include <opencv/cv.h>
#include <opencv2/highgui/highgui.hpp>

#include <memory>

namespace guardian {

class Monitor {
public:
  Monitor();

private:
  cv::VideoCapture capture_;
  cv::Mat background_;

  std::unique_ptr<event::Action> processor_;
  std::unique_ptr<event::Timer> timer_;

  std::unique_ptr<stats::RateStat> statsFrames_;
  std::unique_ptr<stats::RateStat> statsMotionLevel_;

  void doProcess();

private:
  Monitor(Monitor const& copy) = delete;
  Monitor& operator=(Monitor const& copy) = delete;

  Monitor(Monitor&& move) = delete;
  Monitor& operator=(Monitor&& move) = delete;
};
}
