# rcreceiver

FreeBSD RC receiver daemon for my gpiorcrecv-kmod driver.

## About

The daemon is designed to interact with a `rcrecv.ko` kernel driver.
Get the driver from there
[gpiorerecv-kmod](https://gitlab.com/alexandermishin13/gpiorcrecv-kmod).
I wrote them both for my aquariums. Their light and air managed by my another
project [relay-pi-webui](https://gitlab.com/alexandermishin13/relay-pi-webui).
But once I think it would be good to have a remote control to turn the light
on and off instead of do it with a web interfaces buttons.
I never got how to use gpio interrupts from user programs and there they are,
a kernel driver and a daemon.

## Description

A `rcrecv-daemon` waits in a loop for an event, poll(2) or kqueue(2), from a
kernel driver `rcrecv.ko` and after it was came reads a code received by the
driver from its character device. For that code which was configured for some action on a gpio pin the
daemon does that action. Possible actions is set, unset or toggle a pin. In
my case it is the same pin which uses for a management of a lights or an air
commpressor.

## Installation

First of all You need a kernel driver `rcrecv.ko` installed.
See its `readme.md` to build, install, configure and load it.

Then You can build and install the daemon:
```
make
sudo make install
```
This commands will install a daemon itself, its service script
and its man page. You need also a service config file, example of one
is placed in `./rc.conf.d/`. Edit it to suit Your remote control and
copy it as `rcrecv_daemon` to `/usr/local/etc/rc.conf.d/` or just place
its code into `/etc/rc.conf` (old school).
