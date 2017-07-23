#pragma once

namespace event {

class Condition {
public:
  Condition();

  bool value;

private:
  Condition(Condition const& copy) = delete;
  Condition& operator=(Condition const& copy) = delete;

  Condition(Condition const&& move) = delete;
  Condition& operator=(Condition const&& move) = delete;
};

}
