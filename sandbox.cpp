#include <event/Timer.h>
#include <event/Trigger.h>

#include <networking/TCPServer.h>
#include <networking/TCPSocket.h>

#include <common/Logger.h>

#include <unistd.h>

#include <chrono>
#include <iomanip>
#include <iostream>

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  event::EventLoop loop;

  if (argc > 1) {
    // server
    networking::TCPServer server;
    server.bind(9292);

    event::Trigger::arm({server.canAccept()}, [&server]() {
      auto client = server.accept();
      Byte buffer[1024];

      while (true) {
        client.read(buffer, 1024);
      }

      L() << "GOT A CLIENT! " << std::endl;
    });

    loop.run();
  } else {
    // client
    networking::TCPSocket client;
    client.connect(networking::SocketAddress("jiahao-a", 9292));

    Byte buffer[1024];
    client.write(buffer, sizeof(buffer));

    L() << "CONNECTED" << std::endl;

    event::Action sender({client.canWrite()});

    size_t sent = 0;
    sender.callback = [&client, buffer = (Byte*)buffer, &sent ]() {
      size_t written = 0;
      size_t count = 0;
      while ((written = client.write(buffer, 1024)) != 0) {
        sent += written;
        count += 1;

        if (count == 100) {
          break;
        }
      }
    };

    event::Timer timer(0s);
    event::Action reporter({timer.didFire()});

    reporter.callback = [&timer, &sent]() {
      L() << std::fixed << std::setprecision(2)
          << (1.0 * sent / 1024 / 1024 / 128) << std::endl;
      sent = 0;
      timer.extend(1s);
    };

    loop.run();
  }

  return 0;
}
