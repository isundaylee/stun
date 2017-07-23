#include <Util.h>
#include <CommandCenter.h>

#include <ev/ev++.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <vector>

using namespace stun;

int main(int argc, char* argv[]) {
  ev::default_loop loop;

  bool server = false;
  if (argc > 1 && (strcmp(argv[1], "server") == 0)) {
    server = true;
  }

  LOG() << "Running as " << (server ? "server" : "client") << std::endl;
  CommandCenter center;
  if (server) {
    center.serve(2859);
  } else {
    center.connect("127.0.0.1", 2859);
  }

  loop.run(0);

  return 0;
}
