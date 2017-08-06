#!/bin/bash

version="0.4.0"

if [ $(uname) == "Linux" ]; then
  cd $HOME
  sudo apt update
  sudo apt install unzip
  cd /tmp
  wget "https://github.com/isundaylee/stun/releases/download/v$version/stun-linux.zip" -O stun.zip
  unzip stun.zip
  sudo mv stun /usr/local/bin
  echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
else
  echo "Please use homebrew to install stun: brew install isundaylee/repo/stun"
  exit 1
fi
