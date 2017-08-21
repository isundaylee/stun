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

#import <iostream>

@interface AppDelegate ()

@end

@implementation AppDelegate

@synthesize window;

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

    UIViewController *viewController =
        [[AppViewController alloc] initWithHelloString:@"Hello!"];

    self.window.rootViewController = viewController;
    [self.window makeKeyAndVisible];
    
    event::EventLoop::getCurrentLoop().runOnce();
    L() << "==== Ain't I a beautiful log message? ===" << std::endl;
    
    // Override point for customization after application launch.
    return YES;
}

@end
