# stun

[![GitHub version](https://badge.fury.io/gh/isundaylee%2Fstun.svg)](https://badge.fury.io/gh/isundaylee%2Fstun)
[![Build Status](https://travis-ci.org/isundaylee/stun.svg?branch=master)](https://travis-ci.org/isundaylee/stun)

`stun` (Simple TUNnel), as its name stands, is a simple layer-3 network tunnel written in C++. It allows you to easily establish a network tunnel between two computers, and route packets through them.

Each `stun` tunnel consists of two ends: 1) the server, which listens for incoming tunneling requests, and 2) the client, which connects to a server to establish a tunnel. A `stun` server is also capable of serving as a router that provides Internet access for its clients via itself.

Currently `stun` supports the following platforms:

- Linux (client and server)
- macOS (client only)
- iOS (client only)
- FreeBSD (client only)

## Installation

### macOS

`stun` can be obtained on macOS via [Homebrew](https://brew.sh). Once you have Homebrew installed, you can install `stun` with:

```
brew install isundaylee/repo/stun
```

Note that currently `stun` can only be used as a client on macOS (see the introduction paragraph for the distinction).

### Ubuntu

The following installation script has been tested on Ubuntu 14.04 and 16.04:

```
curl -L https://raw.githubusercontent.com/isundaylee/stun/master/bootstrap.sh | bash
```

It will download `stun` and install the binary to `/usr/local/bin`. Note that it will also enable IP forwarding on the machine (which is necessary if you want to use the machine as a `stun` server to provide Internet accesses to your clients). If you don't want IP forwarding enabled, you can use the following command to disable it again after running the bootstrap script:

```
echo 0 | sudo tee /proc/sys/net/ipv4/ip_forward
```

Also note that the install script only enables IP forwarding temporarily until the next reboot. To enable it permanently, edit `/etc/sysctl.conf` and set `net.ipv4.ip_foward` to `1`.

### Compile from source

`stun` uses the [Buck](https://buckbuild.com) build system. Once you have Buck set up for your system, use the following command at the root directory of the repo to compile a release version of `stun`:

```
buck build :main#release
```

It is recommended to build `stun` with clang 4.0 or above. To point Buck to use a specific `clang++` binary, create a file named `.buckconfig.local` in the root directory of the repo with the following content:

```
[cxx]
  cxxpp = /path/to/clang++
  cpp = /path/to/clang++
  cxx = /path/to/clang++
  ld = /path/to/clang++
```

## Usage

To start `stun`, simply do

```
sudo stun
```

Upon the first time you start `stun`, you will be prompted to create a config file. Simply answer the prompt questions (note that the secret used on the client end needs to match the secret used on the server end), and a config file would be automatically generated for you at `~/.stunrc`.

For the client end, the default configuration file would forward all Internet traffic through the `stun` tunnel. To selectively forward traffic, manually edit the `forward_subnets` and `exclude_subnets` entries in the generated config file at `~/.stunrc`. The config file is in standard JSON format.
