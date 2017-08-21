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
#import <event/EventLoop.h>
#import <event/Action.h>
#import <event/Timer.h>
#import <networking/TCPSocket.h>

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

    networking::TCPSocket client;
    client.connect(networking::SocketAddress("192.168.0.53", 3919));
    
    std::unique_ptr<event::Action> reader(new event::Action({client.canRead()}));
    reader->callback = [&client, &reader]() {
        Byte buffer[4000];
        try {
            int read = client.read(&buffer[0], 4000);
        
            if (read > 0) {
                LOG_I("TCP") << "Received " << read << " bytes" << std::endl;
            }
        } catch (networking::SocketClosedException const& ex) {
            LOG_I("TCP") << "Disconnected" << std::endl;
            reader.reset();
        }
    };
    
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
