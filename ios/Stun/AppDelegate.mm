/*
 * Copyright 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <NetworkExtension/NetworkExtension.h>

#import "AppDelegate.h"
#import "AppViewController.h"

#import <common/Logger.h>
#import <stun/Client.h>

#import <iostream>
#import <memory>

using namespace std::chrono_literals;

@interface AppDelegate ()

- (void)doStunEventLoop;
- (void)setupStunEventLoop;

@end

@implementation AppDelegate

@synthesize window;

- (void)doStunEventLoop {
  LOG_I("Loop") << "Stun event loop thread spawned." << std::endl;

  std::unique_ptr<stun::Client> client;
  stun::ClientConfig config{networking::SocketAddress("adp.mit.edu", 2859),
                            false,
                            "lovely",
                            0,
                            std::chrono::seconds(0),
                            "mbp",
                            {},
                            {}};
  client.reset(new stun::Client(config));

  event::EventLoop::getCurrentLoop().run();
}

- (void)setupStunEventLoop {
  common::Logger::getDefault("").setLoggingThreshold(common::LogLevel::VERBOSE);
  LOG_I("Loop") << "Setting up stun event loop." << std::endl;

  [self performSelectorInBackground:@selector(doStunEventLoop) withObject:nil];
}

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

  UIViewController *viewController =
      [[AppViewController alloc] initWithHelloString:@"Hello!"];

  self.window.rootViewController = viewController;
  [self.window makeKeyAndVisible];

  [self setupStunEventLoop];

  // Override point for customization after application launch.
  return YES;
}

@end
