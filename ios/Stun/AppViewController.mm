/*
 * Copyright 2015-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import "AppViewController.h"

#import <NetworkExtension/NetworkExtension.h>

@interface AppViewController ()

@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UIButton *connectButton;

@end

@implementation AppViewController {
}

- (void)doConnect {
  self.statusLabel.text = @"Connecting...";
}

- (void)doStartConnection {
  self.statusLabel.text = @"Creating profile...";

  NETunnelProviderManager *manager = [[NETunnelProviderManager alloc] init];
  NETunnelProviderProtocol *protocol = [[NETunnelProviderProtocol alloc] init];
  protocol.providerBundleIdentifier = @"me.ljh.stunapp.packet-tunnel";
  protocol.providerConfiguration = @{};
  protocol.serverAddress = @"stun.ljh.me";
  manager.protocolConfiguration = protocol;
  [manager saveToPreferencesWithCompletionHandler:^(NSError *_Nullable error) {
    if (error != NULL) {
      NSLog(@"%@", error);
      self.statusLabel.text = @"Failed to create profile";
      return;
    }

    [self doConnect];
  }];
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor whiteColor];

  // Sets up the status label
  self.statusLabel = [[UILabel alloc]
      initWithFrame:CGRectMake(0.0f, 0.0f, self.view.bounds.size.width, 80.0f)];
  self.statusLabel.text = @"Not Connected";
  self.statusLabel.textAlignment = NSTextAlignmentCenter;
  self.statusLabel.font = [UIFont systemFontOfSize:20.0f];
  self.statusLabel.adjustsFontSizeToFitWidth = YES;

  // Sets up the connect button
  self.connectButton = [UIButton buttonWithType:UIButtonTypeRoundedRect];
  self.connectButton.frame =
      CGRectMake(0.0f, 0.0f, self.view.bounds.size.width, 80.0f);
  self.connectButton.titleLabel.font = [UIFont systemFontOfSize:24.0f];
  [self.connectButton addTarget:self
                         action:@selector(doStartConnection)
               forControlEvents:UIControlEventTouchUpInside];
  [self.connectButton setTitle:@"Connect" forState:UIControlStateNormal];

  // Adds the subviews
  [self.view addSubview:self.statusLabel];
  [self.view addSubview:self.connectButton];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  self.statusLabel.center = CGPointMake(self.view.frame.size.width * 0.5f,
                                        self.view.frame.size.height * 0.4f);
  self.connectButton.center = CGPointMake(self.view.frame.size.width * 0.5f,
                                          self.view.frame.size.height * 0.7f);
}

@end
