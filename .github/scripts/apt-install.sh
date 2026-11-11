#!/usr/bin/env bash
set -uo pipefail

opts=(-o Acquire::Retries=5 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30)

for attempt in 1 2 3 4 5; do
    if sudo apt-get "${opts[@]}" update && sudo DEBIAN_FRONTEND=noninteractive apt-get "${opts[@]}" install -y "$@"; then
        exit 0
    fi
    echo "apt failed (attempt ${attempt} of 5), retrying in $((attempt * 20))s"
    sleep $((attempt * 20))
done

echo "::error::apt could not install: $*"
exit 1
