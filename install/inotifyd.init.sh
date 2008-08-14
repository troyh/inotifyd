#!/bin/sh -e

# Start or stop inotifyd
#
# Written by Troy Hakala (troy.hakala@gmail.com)

### BEGIN INIT INFO
# Provides:          postfix mail-transport-agent
# Required-Start:    $local_fs $remote_fs $syslog $time
# Required-Stop:     $local_fs $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: start and stop inotifyd
# Description:       inotifyd is a file system notification logging daemon
### END INIT INFO

PATH=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/usr/sbin/inotifyd
NAME=inotifyd
TZ=
unset TZ

test -x $DAEMON && test -f /etc/inotify.conf || exit 0

. /lib/lsb/init-functions

PIDFILE="/var/run/inotifyd.pid"

running() {
    if [ -f $PIDFILE ]; then
	PID=$(sed 's/ //g' $PIDFILE)
	PROCNAME=`ps -p $PID | sed -e1d | awk '{print $4}'`
	if [ "X$PROCNAME" = "Xinotifyd" ]; then
	    echo y
	fi
    fi
}

case "$1" in
    start)
	log_daemon_msg "Starting inotifyd" inotifyd
	RUNNING=$(running)
	if [ -n "$RUNNING" ]; then
	    log_end_msg 0
	else
	    if start-stop-daemon --start --exec ${DAEMON} -- ; then
			log_end_msg 0
	    else
			log_end_msg 1
	    fi
	fi
    ;;

    stop)
	RUNNING=$(running)
	log_daemon_msg "Stopping inotifyd" inotifyd
	if [ -n "$RUNNING" ]; then
	    if start-stop-daemon --stop --exec ${DAEMON}; then
			log_end_msg 0
	    else
			log_end_msg 1
	    fi
	else
	    log_end_msg 0
	fi
    ;;

    restart)
        $0 stop
        $0 start
    ;;
    
    status)
	RUNNING=$(running)
	if [ -n "$RUNNING" ]; then
	   log_success_msg "inotifyd is running"
	   exit 0
	else
	   log_success_msg "inotifyd is not running"
	   exit 3
	fi
    ;;

    *)
	log_action_msg "Usage: /etc/init.d/inotifyd {start|stop|restart|status}"
	exit 1
    ;;
esac

exit 0
