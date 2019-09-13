#include "gtest/gtest.h"

#include "TestUtils.h"

#include <event/Action.h>

TEST(ActionTests, AlwaysOnAction) {
  event::EventLoop loop;

  auto trigger = TestTrigger{};
  auto action = loop.createAction({});
  action->callback = [&trigger]() { trigger.fire(); };

  loop.runOnce();
  ASSERT_TRUE(trigger.hasFired()) << "Trigger should have fired.";
}

TEST(ActionTests, SingleBaseConditionAction) {
  event::EventLoop loop;

  auto trigger = TestTrigger{};
  auto condition = loop.createBaseCondition();
  auto action = event::Action{loop, {condition.get()}};
  action.callback = [&trigger]() { trigger.fire(); };

  loop.runOnce();
  ASSERT_FALSE(trigger.hasFired()) << "Trigger should not have fired.";

  condition->fire();
  loop.runOnce();
  ASSERT_TRUE(trigger.hasFired()) << "Trigger should have fired.";
}

TEST(ActionTests, MultipleBaseConditionsAction) {
  event::EventLoop loop;

  auto trigger = TestTrigger{};
  auto condition1 = loop.createBaseCondition();
  auto condition2 = loop.createBaseCondition();
  auto action = event::Action{loop, {condition1.get(), condition2.get()}};
  action.callback = [&trigger]() { trigger.fire(); };

  loop.runOnce();
  ASSERT_FALSE(trigger.hasFired()) << "Trigger should not have fired.";

  condition1->fire();
  loop.runOnce();
  ASSERT_FALSE(trigger.hasFired())
      << "Trigger should not have fired after condition 1 fires.";

  condition2->fire();
  loop.runOnce();
  ASSERT_TRUE(trigger.hasFired())
      << "Trigger should have fired after both conditions fire.";
}