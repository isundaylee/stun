# v0.9.0 (2018-12-24)

- [macOS] Adds the ability to automatically configure and restore DNS settings
  based on the server's pushed configuration.

# v0.8.4 (2018-07-14)

- [macOS] Automatically removes existing route to destination host to work
  around the bug in macOS where stale routes from previous stun runs might
  stick around.

# v0.8.3 (2017-12-16)

- [Core] Adds client/server-side MTU setting.

# v0.8.2 (2017-12-01)

- [Core] Better handling for non-recoverable errors.
- [Core] Support for NAT64 carrier networks.
- [BSD] Initial working BSD version.
- [iOS] Implements error reporting.

# v0.8.1 (2017-11-23)

- [Core] Fixes "0.0.0.0" IP addresses being handed out.
- [iOS] Initial working iOS app.
