#include <event/Trigger.h>
#include <networking/UDPSocket.h>

#include <unistd.h>

#include <iostream>

int main(int argc, char* argv[]) {
  event::EventLoop loop;
  networking::UDPSocket socket;
  socket.bind(7000);
  while (true) {
    networking::UDPPacket p;
    if (socket.read(p))
      socket.write(std::move(p));
  }
  loop.run();
  return 0;
}
