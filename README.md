# rcreceiver

FreeBSD RC receiver daemon for my gpiorcrecv-kmod driver.

## About

The daemon is designed to interact with a `rcrecv.ko` kernel driver.
You can get the driver from there
[gpiorerecv-kmod](https://gitlab.com/alexandermishin13/gpiorcrecv-kmod).
I wrote them both for my aquariums. Their light and air managed by another my
project [relay-pi-webui](https://gitlab.com/alexandermishin13/relay-pi-webui).
But once I think it would be good to have a remote control to turn the light
on and off instead of do it with a web interfaces buttons.
I never got how to use gpio interrupts from user programs and there they are,
a kernel driver and a daemon.

## Description

A `rcrecv-daemon` listens for a poll(2) or kqueue(2) event from a `rcrecv.ko`
kernel driver which means that the driver have a code for the daemon. After the
event occurs the daemon reads the code from character device `/dev/rcrecv`.
For these codes, the daemon performs the action that was configured when the
one was started.
Possible actions are set, unset or toggle a pin. In my case it is the same
pins which I had configured for an automatic management of lights and an air
compressor, so that pins can be switched both with `cron` and with a remote
control.
If several consecutive codes is all identical, then after the first successive
code the next ones are ignored if they follows too fast. The minimum interval
between two identical codes is 1000ms and can be changed. This prevents the
relay from switching too quickly when the remote control button is held
pressed.

## Installation

First of all You need a kernel driver `rcrecv.ko` (see above) installed.
See its `README.md` to build, install, configure and load it.

Then You can build and install the daemon:
```
make
sudo make install
```
This commands will install a daemon itself, its service script
and its man page. You need also a service config file, example of one
You can find in `./rc.conf.d/`. Edit it to suit Your remote control codes and
copy it as `rcrecv_daemon` (underscore, not a hyphen) to
`/usr/local/etc/rc.conf.d/` or just place its code into `/etc/rc.conf` (old
school).

You can read alse man pages of the daemon and kernel driver:
```
man 8 rcrecv-daemon
man 4 rcrecv
```

## Bugs
After reloading the `rcrecv.ko` kernel driver when the` rcrecv-daemon`
service is still running, the next restart of the service ends with an OS
restart.

I think about it.
