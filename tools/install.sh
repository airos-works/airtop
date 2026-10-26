#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
AIRTOP_BUILD=${1:-build}
AIRTOP_APPS="$HOME/config/non-packaged/apps/AirTop"
AIRTOP_MENU="$HOME/config/non-packaged/data/deskbar/menu/Applications"
mkdir -p "$AIRTOP_APPS" "$AIRTOP_MENU"
cp "$AIRTOP_BUILD/AirTop" "$AIRTOP_APPS/AirTop.new"
mv -f "$AIRTOP_APPS/AirTop.new" "$AIRTOP_APPS/AirTop"
cp README.md "$AIRTOP_APPS/ReadMe.txt"
cp LICENSE "$AIRTOP_APPS/LICENSE"
mkdir -p "$AIRTOP_APPS/docs"
cp docs/THERMALS.md "$AIRTOP_APPS/docs/THERMALS.md"
mimeset -f "$AIRTOP_APPS/AirTop"
ln -sfn "$AIRTOP_APPS/AirTop" "$AIRTOP_MENU/AirTop"
printf 'Installed %s\n' "$AIRTOP_APPS/AirTop"
