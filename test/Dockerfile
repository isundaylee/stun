FROM ubuntu:bionic

RUN apt-get update
RUN apt-get install -qq iptables iputils-ping net-tools iproute2

RUN mkdir -p /usr/src
WORKDIR /usr/src

COPY stun /usr/src/stun
RUN chmod +x /usr/src/stun

CMD ["/usr/src/stun", "-c", "/usr/config/stunrc"]
