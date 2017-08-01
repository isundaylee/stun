#!/bin/bash

cd $HOME
sudo apt update
sudo apt install unzip
wget "https://github.com/isundaylee/stun/releases/download/v0.2/stun.zip" -O stun.zip
unzip stun.zip
mv stunrc_server.example .stunrc
rm stunrc_client.example
echo 1 | sudo tee /proc/sys/net/ipv4/ip_forward
