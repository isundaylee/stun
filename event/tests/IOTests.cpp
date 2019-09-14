#include <gtest/gtest.h>

#include "TestUtils.h"

#include <event/Action.h>
#include <event/EventLoop.h>
#include <event/IOCondition.h>

TEST(IOTests, CanReadWriteTest) {
  event::EventLoop loop;
  int pipeFds[2];

  int ret = pipe(pipeFds);
  ASSERT_EQ(ret, 0) << "pipe() failed";

  auto readFd = pipeFds[0], writeFd = pipeFds[1];
  auto readCondition = loop.getIOConditionManager().canRead(readFd);
  auto writeCondition = loop.getIOConditionManager().canWrite(readFd);
  auto dummyAction = loop.createAction({readCondition, writeCondition});
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
