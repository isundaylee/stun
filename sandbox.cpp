#include <common/Util.h>
#include <event/Promise.h>
#include <event/Trigger.h>

#include <iostream>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  event::Promise<int> promise;

  event::Trigger::arm({promise.isReady()}, [&promise]() {
    L() << "TRIGGERED!!!!!!!! VALUE: " << promise.consume() << std::endl;
  });

  event::Trigger::performIn(1s, [&promise]() {
    L() << "TRIGGERING!!" << std::endl;
    promise.fulfill(42);
  });

  event::EventLoop::getCurrentLoop().run();

  return 0;
}
