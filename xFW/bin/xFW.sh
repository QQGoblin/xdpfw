#!/bin/bash
set -euo pipefail

if (( EUID != 0 )) || [[ /proc/self/ns/mnt -ef /proc/1/ns/mnt ]]; then
    echo "Use systemctl to control xfw on the host." >&2
    exit 1
fi

export PATH="/opt/tempesta/bin:/opt/tempesta/sbin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
exec /opt/tempesta/bin/xfwctl "$@"
