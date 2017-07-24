#include <common/Util.h>
#include <CommandCenter.h>

#include <event/EventLoop.h>

#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

using namespace stun;

int main(int argc, char* argv[]) {
  event::EventLoop loop;

  bool server = false;
  if (argc > 1 && (strcmp(argv[1], "server") == 0)) {
    server = true;
  }

  LOG() << "Running as " << (server ? "server" : "client") << std::endl;
  CommandCenter center;
  if (server) {
    center.serve(2859);
  } else {
    // center.connect("54.174.137.123", 2859);
    center.connect("127.0.0.1", 2859);
  }

  loop.run();

  return 0;
}
