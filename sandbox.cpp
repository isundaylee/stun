#include <event/Condition.h>
#include <event/Timer.h>
#include <event/Trigger.h>

#include <fcntl.h>
#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  event::EventLoop loop;

  event::Timer timerA(1000);
  event::Timer timerB(3000);

  event::ComputedCondition cond;
  cond.expression = [&timerA, &timerB]() {
    return timerA.didFire()->eval() && timerB.didFire()->eval();
  };

  event::Trigger::arm({&cond}, []() { std::cout << "FIRED!!!" << std::endl; });

  loop.run();

  return 0;
}
