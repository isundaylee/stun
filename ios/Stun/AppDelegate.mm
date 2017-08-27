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

@interface AppDelegate ()

@end

@implementation AppDelegate

@synthesize window;

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];

  UIViewController *viewController = [[AppViewController alloc] init];
  self.window.rootViewController = viewController;
  [self.window makeKeyAndVisible];

  return YES;
}

@end
