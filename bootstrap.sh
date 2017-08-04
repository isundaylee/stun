#!/bin/bash

version="0.3"

if [ $(uname) == "Linux" ]; then
  cd $HOME
  sudo apt update
  sudo apt install unzip
  wget "https://github.com/isundaylee/stun/releases/download/v$version/stun-linux.zip" -O stun.zip
  unzip stun.zip
  mv stunrc_server.example .stunrc
  rm stunrc_client.example
  echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
else
  cd $HOME
  curl "https://github.com/isundaylee/stun/releases/download/v$version/stun-osx.zip" -o stun.zip
  unzip stun.zip
  mv stunrc_client.example .stunrc
  rm stunrc_server.example
fi
