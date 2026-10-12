#!/bin/sh
if [ "$1" = "0" ]; then
    rm -f /etc/ld.so.conf.d/fizmo.conf
fi
/sbin/ldconfig || true
