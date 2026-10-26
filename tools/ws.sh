#!/usr/bin/env bash
set -euo pipefail
exec ssh -F "${AIRTOP_SSH_CONFIG:-/mnt/HaikuWork/x399/ssh/config}" "${AIRTOP_HOST:-ws-haiku}" "$@"
