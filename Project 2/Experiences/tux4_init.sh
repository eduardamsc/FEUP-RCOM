#!/bin/sh

# TUX4

# Set initial settings.
#/etc/init.d/networking restart
ifconfig eth0 down
ifconfig eth1 down
ifconfig eth0 up
ifconfig eth1 up

# Set IP's
ifconfig eth0 172.16.10.254/24
ifconfig eth1 172.16.11.253/24

# Setup as router
echo 1 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

# Set commercial router as default GW
route add default gw 172.16.11.254
