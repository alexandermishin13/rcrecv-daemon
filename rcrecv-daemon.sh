#!/bin/sh
#
# $FreeBSD: 2021-05-21 21:24500Z mishin $
#
# PROVIDE: rcrecvn
# REQUIRE: DAEMON
# KEYWORD: nojail shutdown
#
# Add the following lines to /etc/rc.conf to enable rcrecvn:
#
# rcrecv_enable (bool):	Set to "NO"  by default.
#				Set to "YES" to enable bh1750_daemon.
# rcrecv_daemon_codes (str):	Set to "" by default.
#				Define your profiles here.
# rcrecv_daemon_flags (str):	Set to "" by default.
#				Extra flags passed to start command.
#
# For a profile based configuration use variables like this:
#
# rcrecv_daemon_codes="XXX YYY"
# rcrecv_XXX_pin=<pin1>
# rcrecv_XXX_state=[s|u|t] # set|unset|toggle. Default is "t"
# rcrecv_YYY_pin=<pin2>
# rcrecv_YYY_state=[s|u|t]

. /etc/rc.subr

name=rcrecv_daemon
rcvar=rcrecv_daemon_enable

load_rc_config $name

: ${rcrecv_daemon_enable:="NO"}
: ${rcrecv_daemon_flags:="-b"}

rcrecv_daemon_bin="/usr/local/sbin/rcrecv-daemon"

codes_flags=""
for code in $rcrecv_daemon_codes; do
	eval code_pin="\${rcrecv_${code}_pin}"
	eval code_state="\${rcrecv_${code}_state:-t}"
	codes_flags="${codes_flags} -${code_state} code=${code},pin=${code_pin}"
done

command=${rcrecv_daemon_bin}
command_args="${codes_flags} ${rcrecv_daemon_flags}"

run_rc_command "$@"
