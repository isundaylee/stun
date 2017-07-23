#include "event/EventLoop.h"
#include "event/Action.h"
#include "event/Condition.h"
#include "event/FIFO.h"

#include <iostream>

int main(int argc, char* argv[]) {
  event::EventLoop loop;
  event::FIFO<int> fifoA(1), fifoB(1);
  event::Action actA({fifoA.canPush(), fifoB.canPop()});
  event::Action actB({fifoB.canPush(), fifoA.canPop()});
  fifoB.push(1);

  actA.setCallback([&]() {
    std::cout << "A is invoked! " << std::endl;
    fifoA.push(fifoB.pop());
  });

  actB.setCallback([&]() {
    std::cout << "B is invoked! " << std::endl;
    fifoB.push(fifoA.pop());
  });

  loop.run();

  return 0;
}
