#import "PacketTunnelProvider.h"

#import "event/EventLoop.h"
#import "event/Trigger.h"
#import "stun/Client.h"

@interface PacketTunnelProvider ()

@property BOOL stopped;

@end

@implementation PacketTunnelProvider

using namespace std::chrono_literals;

- (NSError *)errorFromCppException:(std::exception const &)ex {
  NSError *error = [NSError
      errorWithDomain:@"StunPacketTunnel"
                 code:100
             userInfo:@{
               NSLocalizedDescriptionKey : [NSString
                   stringWithFormat:
                       @"Uncaught exception: %@",
                       [NSString stringWithCString:ex.what()
                                          encoding:NSASCIIStringEncoding]]
             }];

  return error;
}

- (void)saveErrorToSharedUserDefaults:(NSError *)error {
  NSUserDefaults *sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:@"group.me.ljh.stunapp"];
  [sharedDefaults setObject:[error localizedDescription] forKey:@"error"];
}

- (void)doStunEventLoop {
  self.stopped = NO;

  common::Logger::getDefault("").tee = ^(std::string message) {
    NSLog(@"%@", @(message.c_str()));
  };

  common::Logger::getDefault("").setLoggingThreshold(common::LogLevel::VERBOSE);
  LOG_I("Loop") << "Starting the stun event loop" << std::endl;

  try {
    while (true) {
      if (self.stopped) {
        return;
      }

      @autoreleasepool {
        event::EventLoop::getCurrentLoop().runOnce();
      }
    }
  } catch (std::exception const &ex) {
    LOG_I("Loop") << "Uncaught exception: " << ex.what() << std::endl;

    NSError *error = [self errorFromCppException:ex];
    [self saveErrorToSharedUserDefaults:error];
    [self cancelTunnelWithError:error];
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
  try {
    [self startStunEventLoop];

    NSLog(@"Options for tunnel: %@", options);

    NSDictionary *vendorData =
        (NSDictionary *)[options objectForKey:@"VendorData"];
    NSString *server = (NSString *)[vendorData objectForKey:@"server"];
    NSString *secret = (NSString *)[vendorData objectForKey:@"secret"];
    NSString *username = (NSString *)[vendorData objectForKey:@"username"];

    if (server == NULL) {
      server = @"";
    }

    if (secret == NULL) {
      secret = @"";
    }

    if (username == NULL) {
      username = @"";
    }

    auto clientConfig = stun::ClientConfig{
        networking::SocketAddress(
            [server cStringUsingEncoding:NSASCIIStringEncoding], kServerPort),
        false,
        [secret cStringUsingEncoding:NSASCIIStringEncoding],
        0,
        0s,
        [username cStringUsingEncoding:NSASCIIStringEncoding],
        {},
        {},
    };

    auto tunnelFactory =
        ^std::shared_ptr<event::Promise<std::unique_ptr<networking::Tunnel>>>(
            stun::ClientTunnelConfig config) {
      auto tunnelPromise = std::make_shared<
          event::Promise<std::unique_ptr<networking::Tunnel>>>();

      // Setting up basic settings
      NEPacketTunnelNetworkSettings *settings =
          [[NEPacketTunnelNetworkSettings alloc]
              initWithTunnelRemoteAddress:@(config.peerTunnelAddr.toString()
                                                .c_str())];
      settings.IPv4Settings = [[NEIPv4Settings alloc]
          initWithAddresses:@[ @(config.myTunnelAddr.toString().c_str()) ]
                subnetMasks:@[
                  @(config.serverSubnetAddr.subnetMask().c_str())
                ]];
      settings.MTU = @(config.mtu);
      settings.DNSSettings =
          [[NEDNSSettings alloc] initWithServers:@[ @"8.8.8.8", @"8.8.4.4" ]];

      settings.IPv4Settings.includedRoutes = @[ [NEIPv4Route defaultRoute] ];
      settings.IPv4Settings.excludedRoutes = @[];

      __weak PacketTunnelProvider *weakSelf = self;

      // Setting up the tunnel
      [self
          setTunnelNetworkSettings:settings
                 completionHandler:^(NSError *_Nullable error) {
                   if (error != NULL) {
                     NSLog(@"Error while setting up network tunnel: %@", error);
                     [self saveErrorToSharedUserDefaults:error];
                     completionHandler(error);
                     return;
                   }

                   auto tunnelSender = ^(networking::TunnelPacket packet) {
                     if (weakSelf == nil) {
                       NSLog(
                           @"Trying to send packets when PacketTunnelProvider "
                           @"is already gone. Ignoring.");
                       return;
                     }

                     auto data = [NSData dataWithBytes:&packet.data[4]
                                                length:packet.size - 4];
                     [weakSelf.packetFlow writePackets:@[ data ]
                                         withProtocols:@[ @(AF_INET) ]];
                   };

                   auto tunnelReceiver = ^std::shared_ptr<event::Promise<
                       std::vector<networking::TunnelPacket>>>() {
                     auto packetsPromise = std::make_shared<event::Promise<
                         std::vector<networking::TunnelPacket>>>();

                     if (weakSelf == nil) {
                       NSLog(@"Trying to receive packets when "
                             @"PacketTunnelProvider "
                             @"is already gone. Ignoring.");
                       packetsPromise->fulfill({});
                       return packetsPromise;
                     }

                     [weakSelf.packetFlow
                         readPacketsWithCompletionHandler:^(
                             NSArray<NSData *> *_Nonnull packets,
                             NSArray<NSNumber *> *_Nonnull protocols) {
                           // Stun is not yet thread-safe. We need to post this
                           // block back to the stun event loop thread.
                           event::Trigger::arm({}, [
                             packets, protocols, packetsPromise
                           ]() {
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

                               // FIXME: Investigate a more sysmetic way of
                               // dealing
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
                               tunnelPacket.fill(packetContent,
                                                 packet.length + 4);
                               tunnelPackets.emplace_back(
                                   std::move(tunnelPacket));

                               free(packetContent);
                             }

                             packetsPromise->fulfill(std::move(tunnelPackets));
                           });
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
  } catch (std::exception const &ex) {
    LOG_I("Loop") << "Uncaught exception while starting the tunnel: "
                  << ex.what() << std::endl;

    NSError *error = [self errorFromCppException:ex];
    [self saveErrorToSharedUserDefaults:error];
    completionHandler(error);
  }
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler {
  self.stopped = YES;
  completionHandler();
}

@end
