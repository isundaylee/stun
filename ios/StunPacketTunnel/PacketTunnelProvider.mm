#import "PacketTunnelProvider.h"

#import "event/EventLoop.h"
#import "stun/Client.h"

@implementation PacketTunnelProvider

using namespace std::chrono_literals;

- (void)doStunEventLoop {
  common::Logger::getDefault("").tee = ^(std::string message) {
    NSLog(@"%@", @(message.c_str()));
  };

  common::Logger::getDefault("").setLoggingThreshold(common::LogLevel::VERBOSE);
  LOG_I("Loop") << "Starting the stun event loop" << std::endl;

  try {
    event::EventLoop::getCurrentLoop().run();
  } catch (std::exception const &ex) {
    LOG_I("Loop") << "Uncaught exception: " << ex.what() << std::endl;
    throw ex;
  }
}

- (void)startStunEventLoop {
  static bool started = false;

  if (!started) {
    started = true;
    [self performSelectorInBackground:@selector(doStunEventLoop)
                           withObject:nil];
  }
}

std::unique_ptr<stun::Client> client;
static const int kServerPort = 2859;

- (void)startTunnelWithOptions:(NSDictionary<NSString *, NSObject *> *)options
             completionHandler:(void (^)(NSError *error))completionHandler {
  [self startStunEventLoop];

  auto clientConfig = stun::ClientConfig{
      networking::SocketAddress("adp.mit.edu", kServerPort),
      false,
      "lovely",
      0,
      0s,
      "ios",
      {},
      {},
  };

  auto tunnelFactory =
      ^std::shared_ptr<event::Promise<std::unique_ptr<networking::Tunnel>>>(
          stun::ClientTunnelConfig config) {
    auto tunnelPromise =
        std::make_shared<event::Promise<std::unique_ptr<networking::Tunnel>>>();

    // Setting up basic settings
    NEPacketTunnelNetworkSettings *settings =
        [[NEPacketTunnelNetworkSettings alloc]
            initWithTunnelRemoteAddress:@(config.peerTunnelAddr.toString()
                                              .c_str())];
    settings.IPv4Settings = [[NEIPv4Settings alloc]
        initWithAddresses:@[ @(config.myTunnelAddr.toString().c_str()) ]
              subnetMasks:@[ @(config.serverSubnetAddr.subnetMask().c_str()) ]];
    settings.MTU = @(config.mtu);
    settings.DNSSettings =
        [[NEDNSSettings alloc] initWithServers:@[ @"8.8.8.8", @"8.8.4.4" ]];

    settings.IPv4Settings.includedRoutes = @[ [NEIPv4Route defaultRoute] ];
    settings.IPv4Settings.excludedRoutes = @[];

    // Setting up the tunnel
    [self
        setTunnelNetworkSettings:settings
               completionHandler:^(NSError *_Nullable error) {
                 if (error != NULL) {
                   NSLog(@"Error while setting up network tunnel: %@", error);
                   completionHandler(error);
                   return;
                 }

                 auto tunnelSender = ^(networking::TunnelPacket packet) {
                   auto data = [NSData dataWithBytes:&packet.data[4]
                                              length:packet.size - 4];
                   [self.packetFlow writePackets:@[ data ]
                                   withProtocols:@[ @(AF_INET) ]];
                 };

                 auto tunnelReceiver = ^std::shared_ptr<
                     event::Promise<std::vector<networking::TunnelPacket>>>() {
                   auto packetsPromise = std::make_shared<
                       event::Promise<std::vector<networking::TunnelPacket>>>();
                   [self.packetFlow
                       readPacketsWithCompletionHandler:^(
                           NSArray<NSData *> *_Nonnull packets,
                           NSArray<NSNumber *> *_Nonnull protocols) {
                         auto tunnelPackets =
                             std::vector<networking::TunnelPacket>{};

                         for (int i = 0; i < protocols.count; i++) {
                           if ([[protocols objectAtIndex:i] intValue] !=
                               AF_INET) {
                             NSLog(@"Ignoring non-IPv4 packet of size "
                                   @"%lu",
                                   [[packets objectAtIndex:0] length]);
                             continue;
                           }

                           // FIXME: Investigate a more sysmetic way of dealing
                           // with headers.

                           auto packet = [packets objectAtIndex:i];
                           Byte *packetContent =
                               (Byte *)malloc(packet.length + 4);

                           assertTrue(packetContent != NULL,
                                      "Out of memory in tunnel receiver. ");

                           packetContent[0] = 0x00;
                           packetContent[1] = 0x00;
                           packetContent[2] = 0x08;
                           packetContent[3] = 0x00;
                           memcpy(&packetContent[4], packet.bytes,
                                  packet.length);

                           auto tunnelPacket = networking::TunnelPacket{};
                           tunnelPacket.fill(packetContent, packet.length + 4);
                           tunnelPackets.emplace_back(std::move(tunnelPacket));

                           free(packetContent);
                         }

                         packetsPromise->fulfill(std::move(tunnelPackets));
                       }];

                   return packetsPromise;
                 };

                 tunnelPromise->fulfill(std::make_unique<networking::Tunnel>(
                     tunnelSender, tunnelReceiver));

                 completionHandler(nil);
               }];

    return tunnelPromise;
  };

  client.reset(new stun::Client(clientConfig, tunnelFactory));
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler {
  completionHandler();
}

@end
