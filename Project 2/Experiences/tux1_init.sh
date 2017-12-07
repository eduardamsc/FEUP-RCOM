#!/bin/sh

# TUX1

# Set initial settings.
/etc/init.d/networking restart

# Set IP's
ifconfig eth0 172.16.10.1
