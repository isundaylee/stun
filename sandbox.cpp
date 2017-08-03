#include <event/Timer.h>
#include <event/Trigger.h>

#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  event::EventLoop loop;
  event::Timer timer(1000);
  event::Action shouter({timer.didFire()});
  shouter.callback = [&timer]() {
    std::cout << "HELLO!!!!!!!!!!!!!!! " << std::endl;
    timer.extend(1000);
  };
  loop.run();
  return 0;
}
