#import "PacketTunnelProvider.h"

@implementation PacketTunnelProvider

- (void)startTunnelWithOptions:(NSDictionary<NSString *, NSObject *> *)options
             completionHandler:(void (^)(NSError *error))completionHandler {
  NSLog(@"======================================================");
  NSLog(@"HERE I AM!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  NSLog(@"======================================================");

  NEPacketTunnelNetworkSettings *settings =
      [[NEPacketTunnelNetworkSettings alloc]
          initWithTunnelRemoteAddress:@"10.100.0.1"];
  settings.IPv4Settings =
      [[NEIPv4Settings alloc] initWithAddresses:@[ @"10.100.0.2" ]
                                    subnetMasks:@[ @"255.255.255.0" ]];
  settings.IPv4Settings.includedRoutes =
      @[ [[NEIPv4Route alloc] initWithDestinationAddress:@"45.55.3.169"
                                              subnetMask:@"255.255.255.255"] ];
  settings.IPv4Settings.excludedRoutes = @[];
  settings.MTU = @1400;

  [self
      setTunnelNetworkSettings:settings
             completionHandler:^(NSError *_Nullable error) {
               NSLog(@"======================================================");
               NSLog(@"HERE I EMERGED!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
               NSLog(@"======================================================");
               completionHandler(nil);
             }];
}

- (void)stopTunnelWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)(void))completionHandler {
  NSLog(@"======================================================");
  NSLog(@"HERE I GO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
  NSLog(@"======================================================");

  completionHandler();
}

@end
