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
