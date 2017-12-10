#!/bin/sh

# TUX2

# Set initial settings.
# /etc/init.d/networking restart
ifconfig eth0 down
ifconfig eth0 up

# Set IP's
ifconfig eth0 172.16.11.1/24

# Set routes
route add -net 172.16.10.0/24 gw 172.16.11.253
route add default gw 172.16.11.254

# These were first set to 0 during Experience 4
echo 1 > /proc/sys/net/ipv4/conf/eth0/accept_redirects
echo 1 > /proc/sys/net/ipv4/conf/all/accept_redirects
