#include <event/Action.h>
#include <event/Condition.h>
#include <event/EventLoop.h>
#include <event/FIFO.h>
#include <event/IOCondition.h>

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

  loop.run();

  return 0;
}
