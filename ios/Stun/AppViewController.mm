#import "AppViewController.h"

#import <NetworkExtension/NetworkExtension.h>

@interface AppViewController ()

@property(nonatomic, strong) UIImageView *logoView;
@property(nonatomic, strong) UITextField *addressField;
@property(nonatomic, strong) UITextField *secretField;
@property(nonatomic, strong) UITextField *usernameField;
@property(nonatomic, strong) UIButton *connectButton;

@property(nonatomic, strong) NETunnelProviderManager *manager;

@end

@implementation AppViewController {
}

- (NSUInteger)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskPortrait;
}

- (BOOL)shouldAutorotate {
  return NO;
}

- (NSString *)getErrorFromSharedUserDefaults {
  NSUserDefaults *sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:@"group.me.ljh.stunapp"];
  [sharedDefaults synchronize];
  return [sharedDefaults objectForKey:@"error"];
}

- (void)clearErrorInSharedUserDefaults {
  NSUserDefaults *sharedDefaults =
      [[NSUserDefaults alloc] initWithSuiteName:@"group.me.ljh.stunapp"];
  [sharedDefaults removeObjectForKey:@"error"];
  [sharedDefaults synchronize];
}

- (void)showAlert:(NSString *)error {
  if (error != NULL) {
    UIAlertController *alertController = [UIAlertController
        alertControllerWithTitle:@"Error"
                         message:error
                  preferredStyle:UIAlertControllerStyleAlert];
    UIAlertAction *okayAction =
        [UIAlertAction actionWithTitle:@"Okay"
                                 style:UIAlertActionStyleDefault
                               handler:nil];
    [alertController addAction:okayAction];
    [self presentViewController:alertController animated:YES completion:nil];
  }
}

- (void)doConfigureAndConnect {
  if (self.manager == NULL) {
    self.manager = [[NETunnelProviderManager alloc] init];
  }

  NETunnelProviderProtocol *protocol = [[NETunnelProviderProtocol alloc] init];
  protocol.providerBundleIdentifier = @"me.ljh.stunapp.packet-tunnel";
  protocol.providerConfiguration = @{
    @"server" : self.addressField.text,
    @"secret" : self.secretField.text,
    @"username" : self.usernameField.text
  };
  protocol.serverAddress = self.addressField.text;
  self.manager.protocolConfiguration = protocol;
  self.manager.enabled = YES;

  [self.manager saveToPreferencesWithCompletionHandler:^(
                    NSError *_Nullable error) {
    if (error != NULL) {
      [self showAlert:[NSString
                          stringWithFormat:@"Error while creating profile: %@",
                                           error]];
      NSLog(@"Error while creating profile: %@", error);

      return;
    }

    [self doConnect];
  }];
}

- (void)doConnect {
  assert(self.manager != NULL);

  [self.manager loadFromPreferencesWithCompletionHandler:^(
                    NSError *_Nullable error) {
    assert(error == NULL);

    NETunnelProviderSession *session =
        (NETunnelProviderSession *)self.manager.connection;
    NSDictionary *options = @{};
    NSError *err = NULL;

    NEVPNStatus status = self.manager.connection.status;
    NSLog(@"Connection status is: %ld", status);

    [self clearErrorInSharedUserDefaults];
    [session startVPNTunnelWithOptions:options andReturnError:&err];

    if (err != NULL) {
      [self showAlert:[NSString
                          stringWithFormat:@"Error while connecting: %@", err]];
      NSLog(@"Error while connecting: %@", err);

      return;
    }
  }];
}

- (void)doStartConnection {
  if (self.manager.connection.status == NEVPNStatusConnected) {
    [self.manager.connection stopVPNTunnel];
  } else {
    [self doConfigureAndConnect];
  }
}

- (void)loadExistingProfile {
  [NETunnelProviderManager loadAllFromPreferencesWithCompletionHandler:^(
                               NSArray<NETunnelProviderManager *>
                                   *_Nullable managers,
                               NSError *_Nullable error) {
    if (error != NULL) {
      [self showAlert:[NSString
                          stringWithFormat:@"Cannot load existing profiles: %@",
                                           error]];
      NSLog(@"Cannot load existing profiles: %@", error);

      return;
    }

    assert(managers != NULL);

    if ([managers count] != 0) {
      assert([managers count] == 1);
      self.manager = [managers objectAtIndex:0];

      NETunnelProviderProtocol *protocol =
          (NETunnelProviderProtocol *)self.manager.protocolConfiguration;
      NSDictionary *options = protocol.providerConfiguration;

      self.addressField.text = (NSString *)[options objectForKey:@"server"];
      self.secretField.text = (NSString *)[options objectForKey:@"secret"];
      self.usernameField.text = (NSString *)[options objectForKey:@"username"];
    }
  }];
}

- (UITextField *)createTextFieldWithPlaceholder:(NSString *)placeholder
                                           type:(UIKeyboardType)type {
  UITextField *field = [[UITextField alloc] init];

  field.translatesAutoresizingMaskIntoConstraints = NO;
  field.placeholder = placeholder;
  field.backgroundColor = UIColor.whiteColor;
  [field.heightAnchor constraintEqualToConstant:45.0].active = YES;
  field.layer.cornerRadius = 4.0;

  UIView *spacerView =
      [[UIView alloc] initWithFrame:CGRectMake(0.0, 0.0, 10.0, 10.0)];
  field.leftViewMode = UITextFieldViewModeAlways;
  field.leftView = spacerView;
  field.keyboardType = type;
  field.autocapitalizationType = UITextAutocapitalizationTypeNone;
  field.autocorrectionType = UITextAutocorrectionTypeNo;

  return field;
}

- (void)onTapBackground:(id)sender {
  [self.view endEditing:YES];
}

- (void)viewDidLoad {
  self.view.backgroundColor = [UIColor colorWithRed:(74.0 / 255.0)
                                              green:(144.0 / 255.0)
                                               blue:(226.0 / 255.0)
                                              alpha:(1.0)];

  // Sets up "tap to dismiss"
  UITapGestureRecognizer *tapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(onTapBackground:)];
  [self.view addGestureRecognizer:tapRecognizer];

  // Sets up the logo view
  self.logoView =
      [[UIImageView alloc] initWithFrame:CGRectMake(0.0, 0.0, 100.0, 100.0)];
  self.logoView.image = [UIImage imageNamed:@"Logo.png"];
  self.logoView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:self.logoView];
  [self.logoView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor]
      .active = YES;
  [self.logoView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                          constant:75.0]
      .active = YES;
  [self.logoView.widthAnchor constraintEqualToConstant:120.0].active = YES;
  [self.logoView.heightAnchor constraintEqualToConstant:120.0].active = YES;

  // Sets up the address text field
  self.addressField = [self createTextFieldWithPlaceholder:@"Server address"
                                                      type:UIKeyboardTypeURL];
  [self.view addSubview:self.addressField];
  [self.addressField.centerXAnchor
      constraintEqualToAnchor:self.view.centerXAnchor]
      .active = YES;
  [self.addressField.topAnchor
      constraintEqualToAnchor:self.logoView.bottomAnchor
                     constant:50.0]
      .active = YES;
  [self.addressField.leftAnchor constraintEqualToAnchor:self.view.leftAnchor
                                               constant:38.0]
      .active = YES;

  // Sets up the secret field
  self.secretField =
      [self createTextFieldWithPlaceholder:@"Secret"
                                      type:UIKeyboardTypeDefault];
  self.secretField.secureTextEntry = YES;
  [self.view addSubview:self.secretField];
  [self.secretField.centerXAnchor
      constraintEqualToAnchor:self.view.centerXAnchor]
      .active = YES;
  [self.secretField.topAnchor
      constraintEqualToAnchor:self.addressField.bottomAnchor
                     constant:23.0]
      .active = YES;
  [self.secretField.leftAnchor constraintEqualToAnchor:self.view.leftAnchor
                                              constant:38.0]
      .active = YES;

  // Sets up the username field
  self.usernameField = [self
      createTextFieldWithPlaceholder:@"Username (leave empty if not using)"
                                type:UIKeyboardTypeDefault];
  [self.view addSubview:self.usernameField];
  [self.usernameField.centerXAnchor
      constraintEqualToAnchor:self.view.centerXAnchor]
      .active = YES;
  [self.usernameField.topAnchor
      constraintEqualToAnchor:self.secretField.bottomAnchor
                     constant:23.0]
      .active = YES;
  [self.usernameField.leftAnchor constraintEqualToAnchor:self.view.leftAnchor
                                                constant:38.0]
      .active = YES;

  // Sets up the connect button
  self.connectButton = [[UIButton alloc] init];
  self.connectButton.translatesAutoresizingMaskIntoConstraints = NO;
  self.connectButton.backgroundColor = [UIColor colorWithRed:(160.0 / 255.0)
                                                       green:(201.0 / 255.0)
                                                        blue:(248.0 / 255.0)
                                                       alpha:1.0];
  self.connectButton.layer.cornerRadius = 4.0;
  [self.connectButton setTitle:@"Connect" forState:UIControlStateNormal];
  [self.view addSubview:self.connectButton];
  [self.connectButton.centerXAnchor
      constraintEqualToAnchor:self.view.centerXAnchor]
      .active = YES;
  [self.connectButton.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor
                     constant:-120.0]
      .active = YES;
  [self.connectButton.widthAnchor constraintEqualToConstant:160.0].active = YES;
  [self.connectButton.heightAnchor constraintEqualToConstant:45.0].active = YES;
  [self.connectButton addTarget:self
                         action:@selector(doStartConnection)
               forControlEvents:UIControlEventTouchUpInside];

  [self loadExistingProfile];
}

- (void)onVPNStatusChange {
  NSString *title = @"";
  BOOL enabled = YES;
  NEVPNStatus status = self.manager.connection.status;

  if (status == NEVPNStatusConnecting) {
    title = @"Connecting...";
    enabled = NO;
  } else if (status == NEVPNStatusConnected) {
    title = @"Disconnect";
    enabled = YES;
  } else if (status == NEVPNStatusDisconnecting) {
    title = @"Disconnecting...";
    enabled = NO;
  } else {
    title = @"Connect";
    enabled = YES;
  }

  NSString *error = [self getErrorFromSharedUserDefaults];
  if (error != NULL) {
    [self showAlert:error];
  }

  [self.connectButton setTitle:title forState:UIControlStateNormal];
  self.connectButton.enabled = enabled;
}

- (void)viewWillDisappear:(BOOL)animated {
  // Unregisters VPN status observer
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)viewDidAppear:(BOOL)animated {
  // Registers VPN status observer
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(onVPNStatusChange)
             name:NEVPNStatusDidChangeNotification
           object:nil];
}

@end
