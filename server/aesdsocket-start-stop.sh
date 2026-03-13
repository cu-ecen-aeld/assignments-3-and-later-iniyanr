#!/bin/sh
NAME=aesdsocket
DAEMON=/usr/bin/$NAME
PIDFILE=/var/run/$NAME.pid

case "$1" in 
	start)
		echo -n "Starting $NAME:"
		start-stop-daemon --start --quiet --exec $DAEMON -- -d
		echo "OK"
		;;
	stop)
		echo -n "Stopping $NAME:"
		start-stop-daemon --stop --quiet --pidfile $PIDFILE --signal TERM  --retry 5
		echo "OK"
		;;
	restart)
		$0 stop
		$0 start
		;;
	*)
		echo "Usage : $0 {start | stop | restart}"

		exit 1
esac
exit 0
