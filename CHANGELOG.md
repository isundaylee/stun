- [Core] Implements TCP data pipe support.

# v0.10.2 (2019-09-10)

- [Core] Fixes infinite prompt stream when configuration wizard is interrupted.
- [Core] Implements packet loss estimator (Issue #8).

# v0.10.1 (2019-09-06)

- [Core] Fixes server crash when provided with malformatted client-provided 
  subnets.
- [Core] Fixes client reconnection crash when DNS settings are not 
  provided/applied.

# v0.10.0 (2019-09-02)

- [Core] Implements client-provided subnet feature. For each subnet that the
  client claims to provide, the server will route all traffic belonging to that
  subnet to the client whenever the client is connected.
- [Core] Fixes the issue where launching a second `stun` server incorrectly
  clears the iptables MASQUERADE rule created by the previous, unrelated `stun`
  server. (Issue #2)
- [Core] Improves timer code's robustness, including fixing a timer-related
  assertion error (Issue #4).

# v0.9.2 (2019-03-03)

- [macOS] Fixes a crash during DNS settings restoration.

# v0.9.1 (2018-12-24)

- [macOS] Adds `accept_dns_pushes` option to the client config wizard.

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
