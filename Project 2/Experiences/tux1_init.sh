#!/bin/sh

# TUX1

# Set initial settings.
#/etc/init.d/networking restart
ifconfig eth0 down
ifconfig eth0 up

# Set IP's
ifconfig eth0 172.16.10.1/24

route add default gw 172.16.10.254

