#!/bin/bash

if [ $# -gt 0 ]; then
    if [ $1'x' == '-9x' ]; then
        kill -9 -`ps -ajx | grep mgx | grep master | awk '{print $2}'`
    else
        kill $1 `ps -ajx | grep mgx | grep master | awk '{print $2}'`
    fi
else
    kill -9 -`ps -ajx | grep mgx | grep master | awk '{print $2}'`
fi


###############################################################################
## log file rotate by date, write next line in /etc/crontab
# 0 0 * * * bash kill.sh -USR1
