#! /bin/sh

ALL=n

if [ $# = 1 ]; then
    ALL=y
fi

echo -en "Linux/ROSE 001. AX.25, NET/ROM and ROSE Users.\r"

if [ -r /proc/net/ax25 ]; then
    cat /proc/net/ax25 | awk '
	BEGIN  {
		printf "Active AX.25 Sessions\r"
		printf "Dest       Source     State\r"
		n = 0
	       }
	NR > 1 {
		if ($4 == 0) {
		    state = "LISTENING"
		} else if ($4 == 1) {
		    state = "CONNECTING"
		} else if ($4 == 2) {
		    state = "DISCONNECTING"
		} else if ($4 == 3) {
		    state = "CONNECTED"
		} else {
		    state = "RECOVERY"
		}
		if ($4 != 0) {
		    printf "%-9s  %-9s  %s\r", $1, $2, state
		    n++
		} else {
		    if (ALL == "y") {
			printf "%-9s  %-9s  %s\r", $1, $2, state
			n++
		    }
		}
	       }
	END    {
		    if (n == 0) {
			printf "None active\r"
		    }
	       }' ALL=$ALL

	echo -en "\r"
fi

if [ -r /proc/net/nr ]; then
    cat /proc/net/nr | awk '
	BEGIN  {
		printf "Active NET/ROM Sessions\r"
		printf "User       Dest       Source     State\r"
		n = 0
	       }
	NR > 1 {
		if ($7 == 0) {
		    state = "LISTENING"
		} else if ($7 == 1) {
		    state = "CONNECTING"
		} else if ($7 == 2) {
		    state = "DISCONNECTING"
		} else if ($7 == 3) {
		    state = "CONNECTED"
		} else {
		    state = "RECOVERY"
		}
		if ($7 != 0) {
		    printf "%-9s  %-9s  %-9s  %s\r", $1, $2, $3, state
		    n++
		} else {
		    if (ALL == "y") {
			printf "%-9s  %-9s  %-9s  %s\r", $1, $2, $3, state
			n++
		    }
		}
	       }
	END    {
		    if (n == 0) {
			printf "None active\r"
		    }
	       }' ALL=$ALL

	echo -en "\r"
fi

if [ -r /proc/net/rose ]; then
    cat /proc/net/rose | awk '
	BEGIN  {
		printf "Active ROSE Sessions\r"
		printf "Dest                   Source                 State\r"
		n = 0
	       }
	NR > 1 {
		if ($7 == 0) {
		    state = "LISTENING"
		} else if ($7 == 1) {
		    state = "CONNECTING"
		} else if ($7 == 2) {
		    state = "DISCONNECTING"
		} else if ($7 == 3) {
		    state = "CONNECTED"
		} else {
		    state = "RESETTING"
		}
		if ($7 != 0) {
		    printf "%-10s  %-9s  %-10s  %-9s  %s\r", $1, $2, $3, $4, state
		    n++
		} else {
		    if (ALL == "y") {
			printf "%-10s  %-9s  %-10s  %-9s  %s\r", $1, $2, $3, $4, state
			n++
		    }
		}
	       }
	END    {
		    if (n == 0) {
			printf "None active\r"
		    }
	       }' ALL=$ALL

	echo -en "\r"
fi

read

exit 0

