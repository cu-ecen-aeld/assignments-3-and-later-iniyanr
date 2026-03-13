#!/bin/sh

# Make sure these are defined correctly
NAME=aesdsocket
DAEMON=/usr/bin/$NAME
PIDFILE=/var/run/$NAME.pid

case "$1" in
    start)
        echo -n "Starting $NAME: "
        # Check if the file actually exists before trying to run it
        if [ ! -f "$DAEMON" ]; then
            echo "FAIL: $DAEMON not found"
            exit 1
        fi
        
        # Use -x to point to the actual binary
        start-stop-daemon -S -q -p $PIDFILE -x $DAEMON -- -d
        echo "OK"
        ;;
    stop)
        echo -n "Stopping $NAME: "
        start-stop-daemon -K -q -p $PIDFILE
        echo "OK"
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac
