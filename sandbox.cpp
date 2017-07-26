#include <event/Action.h>
#include <event/Condition.h>
#include <event/EventLoop.h>
#include <event/FIFO.h>
#include <event/IOCondition.h>
#include <event/Timer.h>
#include <event/Trigger.h>

#include <fcntl.h>
#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  event::EventLoop loop;
  event::FIFO<char> fifo(4);

  // Open the file descriptors
  int fa = open("/tmp/a", O_RDONLY);
  int fb = open("/tmp/b", O_WRONLY);

  // Producer: /tmp/a -> fifo
  event::Action producer(
      {event::IOConditionManager::canRead(fa), fifo.canPush()});
  producer.callback = [&fifo, fa, fb]() {
    char c;
    int ret = read(fa, &c, 1);
    if (ret == 0) {
      event::IOConditionManager::close(fa);
      event::IOConditionManager::close(fb);
      close(fa);
      close(fb);
    } else {
      fifo.push(c);
    }
  };

  // Consumer: fifo -> /tmp/b
  event::Action consumer(
      {fifo.canPop(), event::IOConditionManager::canWrite(fb)});
  consumer.callback = [&fifo, fb]() {
    char c = fifo.pop();
    write(fb, &c, 1);
  };

  // A timer that puts '0' into /tmp/b every 10 ms.
  event::Timer timer(10);
  event::Action speaker({timer.didFire(), event::IOConditionManager::canWrite(fb)});
  speaker.callback = [&timer, fb]() {
    char c = '0';
    write(fb, &c, 1);
    timer.extend(10);
  };

  loop.run();

  return 0;
}
