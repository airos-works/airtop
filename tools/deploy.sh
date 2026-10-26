#!/usr/bin/env bash
# Build natively, validate, and install on the configured Haiku workstation.
set -euo pipefail
cd "$(dirname "$0")/.."
tar -cf - src resources tests tools docs Makefile README.md LICENSE | tools/ws.sh \
    'mkdir -p /boot/home/Develop/AirTop && cd /boot/home/Develop/AirTop && tar -xm -f - && make -j4 && make check && make install'
