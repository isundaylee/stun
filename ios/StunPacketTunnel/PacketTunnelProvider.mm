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

- (void)startTunnelWithOptions:(NSDictionary<NSString *, NSObject *> *)options
             completionHandler:(void (^)(NSError *error))completionHandler {
  [self startStunEventLoop];

  auto clientConfig = stun::ClientConfig{
      networking::SocketAddress("adp.mit.edu", 2859),
      false,
      "lovely",
      0,
      0s,
      "ios",
      {networking::SubnetAddress{networking::IPAddress("45.55.3.169"), 32}},
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

    // Setting up routes to forward/exclude
    NSMutableArray *includedRoutes = [[NSMutableArray alloc] init];
    for (auto const &subnetToForward : config.subnetsToForward) {
      [includedRoutes
          insertObject:[[NEIPv4Route alloc]
                           initWithDestinationAddress:@(subnetToForward.addr
                                                            .toString()
                                                            .c_str())
                                           subnetMask:@(subnetToForward
                                                            .subnetMask()
                                                            .c_str())]
               atIndex:[includedRoutes count]];
    }

    NSMutableArray *excludedRoutes = [[NSMutableArray alloc] init];
    for (auto const &subnetToExclude : config.subnetsToExclude) {
      [excludedRoutes
          insertObject:[[NEIPv4Route alloc]
                           initWithDestinationAddress:@(subnetToExclude.addr
                                                            .toString()
                                                            .c_str())
                                           subnetMask:@(subnetToExclude
                                                            .subnetMask()
                                                            .c_str())]
               atIndex:[excludedRoutes count]];
    }

    settings.IPv4Settings.includedRoutes = [includedRoutes copy];
    settings.IPv4Settings.excludedRoutes = [excludedRoutes copy];

    // Setting up the tunnel
    [self
        setTunnelNetworkSettings:settings
               completionHandler:^(NSError *_Nullable error) {
                 if (error != NULL) {
                   NSLog(@"Error while setting up network tunnel: %@", error);
                   completionHandler(error);
                   return;
                 }

                 NSLog(@"Network tunnel set up");
                 tunnelPromise->fulfill(std::make_unique<networking::Tunnel>(
                     // Sender
                     ^(networking::TunnelPacket packet) {
                       L() << "SENDING PACKET OF SIZE " << packet.size
                           << std::endl;
                     },
                     // Receiver
                     ^std::shared_ptr<event::Promise<
                         std::vector<networking::TunnelPacket>>>() {
                       auto packetsPromise = std::make_shared<event::Promise<
                           std::vector<networking::TunnelPacket>>>();

                       [self.packetFlow
                           readPacketsWithCompletionHandler:^(
                               NSArray<NSData *> *_Nonnull packets,
                               NSArray<NSNumber *> *_Nonnull protocols) {

                             auto tunnelPackets =
                                 std::vector<networking::TunnelPacket>{};

                             for (int i = 0; i < [protocols count]; i++) {
                               if ([[protocols objectAtIndex:i] intValue] !=
                                   AF_INET) {
                                 NSLog(@"Ignoring non-IPv4 packet of size "
                                       @"%lu",
                                       [[packets objectAtIndex:0] length]);
                               }
                             }

                             NSLog(@"Delivering %lu packets.",
                                   tunnelPackets.size());

                             packetsPromise->fulfill(std::move(tunnelPackets));
                           }];

                       return packetsPromise;
                     }));

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
