# rcreceiver

FreeBSD RC receiver daemon for my gpiorcrecv-kmod driver.

## About

The daemon is designed to interact with a `rcrecv.ko` kernel driver.
Get the driver from there
[gpiorerecv-kmod](https://gitlab.com/alexandermishin13/gpiorcrecv-kmod)

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
