#include "gtest/gtest.h"

#include "TestUtils.h"

#include <event/Callback.h>

class TriggerFirer {
public:
  TriggerFirer(TestTrigger& trigger) : trigger_{trigger} {}

  void fireVoid() { trigger_.fire(); }

private:
  TestTrigger& trigger_;
};

TEST(CallbackTests, VoidLambdaFunction) {
  auto trigger = TestTrigger{};
  auto callback = event::Callback<void>{};
  callback = [&trigger]() { trigger.fire(); };

  callback.invoke();
  ASSERT_TRUE(trigger.hasFired()) << "Trigger should have fired.";
}

TEST(CallbackTests, VoidMemberFunction) {
  auto trigger = TestTrigger{};
  auto callback = event::Callback<void>{};
  auto firer = TriggerFirer{trigger};
  callback.setMethod<TriggerFirer, &TriggerFirer::fireVoid>(&firer);

  callback.invoke();
  ASSERT_TRUE(trigger.hasFired()) << "Trigger should have fired.";
}

TEST(CallbackTests, ChangeMemberFunction) {
  auto trigger1 = TestTrigger{};
  auto trigger2 = TestTrigger{};
  auto callback = event::Callback<void>{};
  auto firer1 = TriggerFirer{trigger1};
  auto firer2 = TriggerFirer{trigger2};
  callback.setMethod<TriggerFirer, &TriggerFirer::fireVoid>(&firer1);
  callback.target = &firer2;

  callback.invoke();
  ASSERT_FALSE(trigger1.hasFired()) << "Trigger 1 should not have fired.";
  ASSERT_TRUE(trigger2.hasFired()) << "Trigger 2 should have fired.";
}