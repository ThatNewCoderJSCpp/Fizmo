#!/bin/sh
if [ -d /etc/ld.so.conf.d ] && [ "$(rpm --eval '%%{_lib}' 2>/dev/null)" = "lib64" ]; then
    echo /usr/lib > /etc/ld.so.conf.d/fizmo.conf
fi
/sbin/ldconfig || true
