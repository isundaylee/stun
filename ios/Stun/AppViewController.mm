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

@property(nonatomic, strong) NETunnelProviderManager *manager;

@end

@implementation AppViewController {
}

- (void)doCheckExistingProfiles {
  [NETunnelProviderManager
      loadAllFromPreferencesWithCompletionHandler:^(
          NSArray<NETunnelProviderManager *> *_Nullable managers,
          NSError *_Nullable error) {
        if (error != NULL) {
          NSLog(@"Cannot load existing profiles: %@", error);
          self.statusLabel.text = @"Failed to load profiles";
          return;
        }

        assert(managers != NULL);

        if ([managers count] == 0) {
          [self doCreateProfile];
        } else {
          assert([managers count] == 1);
          self.manager = [managers objectAtIndex:0];

          [self doConnect];
        }
      }];
}

// - (void)doLoadProfile {
//   self.statusLabel.text = @"Loading profile...";
//
//   NETunnelProviderManager *manager = [[NETunnelProviderManager alloc] init];
//   [manager
//       loadFromPreferencesWithCompletionHandler:^(NSError *_Nullable error) {
//         if (error != NULL) {
//           NSLog(@"Error while loading profile: %@", error);
//           self.statusLabel.text = @"Failed to load profile";
//           return;
//         }
//
//         [self doCreateProfileWithManager:manager];
//       }];
// }

- (void)doCreateProfile {
  NETunnelProviderManager *manager = [[NETunnelProviderManager alloc] init];
  NETunnelProviderProtocol *protocol = [[NETunnelProviderProtocol alloc] init];
  protocol.providerBundleIdentifier = @"me.ljh.stunapp.packet-tunnel";
  protocol.providerConfiguration = @{};
  protocol.serverAddress = @"stun.ljh.me";
  manager.protocolConfiguration = protocol;
  manager.enabled = YES;
  [manager saveToPreferencesWithCompletionHandler:^(NSError *_Nullable error) {
    if (error != NULL) {
      NSLog(@"Error while creating profile: %@", error);
      self.statusLabel.text = @"Failed to create profile";
      return;
    }

    self.manager = manager;
    [self doConnect];
  }];
}

- (void)doConnect {
  assert(self.manager != NULL);

  self.statusLabel.text = @"Connecting...";

  [self.manager
      loadFromPreferencesWithCompletionHandler:^(NSError *_Nullable error) {
        assert(error == NULL);

        NETunnelProviderSession *session =
            (NETunnelProviderSession *)self.manager.connection;
        NSDictionary *options = @{};
        NSError *err = NULL;

        NEVPNStatus status = self.manager.connection.status;
        NSLog(@"STATUS IS ++++++++++++++++++++++++++++++++++++++++++++++ %ld",
              status);

        [session startVPNTunnelWithOptions:options andReturnError:&err];

        if (err != NULL) {
          NSLog(@"Error while connecting: %@", err);
          self.statusLabel.text = @"Failed to connect";
          return;
        }
      }];
}

- (void)doStartConnection {
  [self doCheckExistingProfiles];
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
