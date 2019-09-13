#pragma once

class TestTrigger {
public:
  TestTrigger() {}

  TestTrigger(TestTrigger const& rhs) = delete;
  TestTrigger& operator=(TestTrigger const& rhs) = delete;

  void fire() { fired_ = true; }

  bool hasFired() const { return fired_; }

private:
  bool fired_ = false;
};