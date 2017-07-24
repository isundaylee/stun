#include "event/EventLoop.h"
#include "event/Action.h"
#include "event/Condition.h"
#include "event/IOCondition.h"
#include "event/FIFO.h"

#include <unistd.h>
#include <fcntl.h>

#include <iostream>

int main(int argc, char* argv[]) {
  event::EventLoop loop;

  int fd = open("/tmp/a", O_RDONLY);
  event::Action readPipe({event::IOConditionManager::canRead(fd)});
  readPipe.setCallback([fd]() {
    char buffer[256];
    int charRead = read(fd, buffer, sizeof(buffer));
    buffer[charRead] = '\0';
    std::cout << buffer << std::flush;
  });

  loop.run();

  return 0;
}
