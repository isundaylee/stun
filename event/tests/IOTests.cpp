#include <gtest/gtest.h>

#include "TestUtils.h"

#include <event/Action.h>
#include <event/EventLoop.h>
#include <event/IOCondition.h>
#include <event/Timer.h>

TEST(IOTests, CanReadWriteTest) {
  event::EventLoop loop;
  int pipeFds[2];

  int ret = pipe(pipeFds);
  ASSERT_EQ(ret, 0) << "pipe() failed";

  auto readFd = pipeFds[0], writeFd = pipeFds[1];
  auto readCondition = loop.getIOConditionManager().canRead(readFd);
  auto writeCondition = loop.getIOConditionManager().canWrite(writeFd);
  auto dummyAction = loop.createAction("", {readCondition, writeCondition});
  dummyAction->callback = []() {};

  loop.runOnce();
  ASSERT_TRUE(writeCondition->eval()) << "Should be able to write. ";
  ASSERT_FALSE(readCondition->eval()) << "Should not be able to read yet.";

  char buf[] = "hello";
  ret = write(writeFd, buf, sizeof(buf));
  ASSERT_EQ(ret, sizeof(buf)) << "write() failed";
  loop.runOnce();
  ASSERT_TRUE(readCondition->eval()) << "Should now be able to write.";
}

// Tests that an IO condition indirected via a ComputedCondition can trigger
// Action-s correctly.
TEST(IOTests, IndirectIOCondition) {
  event::EventLoop loop;
  int pipeFds[2];

  auto ret = pipe(pipeFds);
  ASSERT_EQ(ret, 0) << "pipe() failed.";

  auto readFd = pipeFds[0], writeFd = pipeFds[1];
  auto readCondition = loop.getIOConditionManager().canRead(readFd);
  auto writeCondition = loop.getIOConditionManager().canWrite(writeFd);
  auto computedCondition = loop.createComputedCondition();
  computedCondition->expression = [readCondition]() {
    return readCondition->eval();
  };
  auto trigger = TestTrigger{};
  auto action = loop.createAction("", {computedCondition.get()});
  action->callback = [&trigger]() { trigger.fire(); };

  loop.runOnce();
  ASSERT_FALSE(trigger.hasFired()) << "Trigger should not have fired yet.";

  char buf[] = "hello";
  ret = write(writeFd, buf, sizeof(buf));
  ASSERT_EQ(ret, sizeof(buf)) << "write() failed";
  loop.runOnce();
  ASSERT_TRUE(trigger.hasFired()) << "Trigger should now have fired.";
  ASSERT_TRUE(readCondition->eval()) << "Trigger should now have fired.";
}

#if TARGET_LINUX
// We can potentially have a ready IO condition but no runnable action. Since we
// have got rid of the concept of "interesting" events, this will busy-loop the
// main event loop to take 100% CPU if we don't do artificial sleeping.
TEST(IOTests, BlockedIOCPUUtil) {
  auto startTime = getCPUTime();

  event::EventLoop loop;

  int pipeFds[2];
  auto ret = pipe(pipeFds);
  ASSERT_EQ(ret, 0) << "pipe() failed.";
  auto readFd = pipeFds[0], writeFd = pipeFds[1];

  auto writeCondition = loop.getIOConditionManager().canWrite(writeFd);
  auto neverCondition = loop.createBaseCondition();
  auto timerCondition = loop.createTimer(std::chrono::milliseconds{100});
  neverCondition->arm();
  auto computedCondition = loop.createComputedCondition();
  computedCondition->expression = [&timerCondition, &writeCondition]() {
    return timerCondition->didFire()->eval() && writeCondition->eval();
  };

  auto trigger = TestTrigger{};
  auto action = loop.createAction("", {computedCondition.get()});
  action->callback = [&trigger]() { trigger.fire(); };

  while (!trigger.hasFired()) {
    loop.runOnce();
  }

  auto endTime = getCPUTime();
  auto cpuTime = endTime - startTime;
  auto cpuTimeMs =
      std::chrono::duration_cast<std::chrono::milliseconds>(cpuTime).count();

  ASSERT_LE(cpuTimeMs, 30)
      << "Average CPU usage should not be greater than 30%";
}
#endif
