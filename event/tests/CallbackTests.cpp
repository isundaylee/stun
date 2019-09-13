#include "gtest/gtest.h"

#include "TestUtils.h"

#include <string>

#include <event/Callback.h>

class TriggerFirer {
public:
  TriggerFirer(TestTrigger& trigger) : trigger_{trigger} {}

  void fireVoid() { trigger_.fire(); }

private:
  TestTrigger& trigger_;
};

class StringKeeper {
public:
  StringKeeper(std::string value) : value_{std::move(value)} {}

  std::string getValue() { return value_; }
  std::string getValueConst() const { return value_; }

private:
  std::string value_;
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

TEST(CallbackTests, LambdaFunctionWithReturnValue) {
  auto callback = event::Callback<std::string>{};

  callback = []() { return "test"; };
  ASSERT_EQ(callback.invoke(), "test")
      << "invoke() should return the expected value.";
}

TEST(CallbackTests, MemberFunctionWithReturnValue) {
  auto callback = event::Callback<std::string>{};
  auto keeper = StringKeeper{"test"};

  callback.setMethod<StringKeeper, &StringKeeper::getValue>(&keeper);
  ASSERT_EQ(callback.invoke(), "test")
      << "invoke() should return the expected value.";
}

TEST(CallbackTests, ConstMemberFunctionWithReturnValue) {
  auto callback = event::Callback<std::string>{};
  auto const keeper = StringKeeper{"test"};

  callback.setMethod<StringKeeper, &StringKeeper::getValueConst>(&keeper);
  ASSERT_EQ(callback.invoke(), "test")
      << "invoke() should return the expected value.";
}

TEST(CallbackTests, ChangeConstMemberFunctionWithReturnValue) {
  auto callback = event::Callback<std::string>{};
  auto const keeper1 = StringKeeper{"test1"};
  auto const keeper2 = StringKeeper{"test2"};

  callback.setMethod<StringKeeper, &StringKeeper::getValueConst>(&keeper1);
  callback.target = &keeper2;
  ASSERT_EQ(callback.invoke(), "test2")
      << "invoke() should return the expected value.";
}