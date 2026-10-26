#!/usr/bin/env bash
# Cross-builds AirTop for arm64 air/OS (Raspberry Pi 4, ROCK 5) with the
# build tree's tools and makes artifacts/airtop-<version>-arm64.hpkg.
#   tools/build-arm64.sh          build and package
#   tools/build-arm64.sh build    only build build-arm64/AirTop
# The auxiliary display API (BAuxDisplay) is newer than the sysroot: its
# header and libdevice.so come from HAIKU_TREE and HAIKU_BUILD.
set -euo pipefail
umask 022
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
WORK=${WORK:-/mnt/HaikuWork}
CROSS=${CROSS:-$WORK/build/arm64/cross-tools-arm64/bin/aarch64-unknown-haiku-}
SYSROOT=${SYSROOT:-$WORK/build/summit-arm64/sysroot}
TOOLS=${TOOLS:-$WORK/build/arm64/objects/linux/x86_64/release/tools}
MIMEDB=${MIMEDB:-$WORK/build/arm64/objects/common/data/mime_db/mime_db}
HAIKU_TREE=${HAIKU_TREE:-$WORK/rpi4/haiku}
HAIKU_BUILD=${HAIKU_BUILD:-$WORK/rpi4/build/objects/haiku/arm64/release}
JOBS=${JOBS:-12}
export TMPDIR=${TMPDIR:-$WORK/tmp}

BUILD=$ROOT/build-arm64
HOSTBIN=$BUILD/.hostbin
mkdir -p "$HOSTBIN"
# The cross compiler links libgcc.a, whose private unwinder libstdc++'s
# __cxa_throw never sees: every C++ exception would end in std::terminate.
UNWIND_SPECS=$HOSTBIN/shared-unwinder.specs
printf '*libgcc:\n-lgcc_s -lgcc\n\n' > "$UNWIND_SPECS"

make -C "$ROOT" -j"$JOBS" BUILD=build-arm64 \
	CXX="${CROSS}g++ --sysroot=$SYSROOT -specs=$UNWIND_SPECS" \
	CPPFLAGS="-I$HAIKU_TREE/headers/os/device -I$HAIKU_TREE/headers/os/drivers" \
	LDFLAGS="-L$HAIKU_BUILD/kits/device" \
	RC="$TOOLS/rc/rc" XRES="$TOOLS/xres" MIMESET=true
[[ ${1:-} == build ]] && exit 0

INFO=$ROOT/resources/AirTop.PackageInfo
NAME=$(awk '$1 == "name" { print $2; exit }' "$INFO")
VERSION=$(awk '$1 == "version" { print $2; exit }' "$INFO")
FILE=$ROOT/artifacts/$NAME-$VERSION-arm64.hpkg
STAGE=$BUILD/stage
[[ -e $STAGE ]] && "$TOOLS/rm_attrs" -rf "$STAGE"
mkdir -p "$STAGE/apps/AirTop" "$STAGE/data/deskbar/menu/Applications" \
	"$STAGE/data/licenses" "$STAGE/documentation/packages/airtop" "$ROOT/artifacts"
cp "$BUILD/AirTop" "$STAGE/apps/AirTop/AirTop"
chmod 755 "$STAGE/apps/AirTop/AirTop"
"${CROSS}strip" --strip-debug "$STAGE/apps/AirTop/AirTop"
"$TOOLS/xres" -o "$STAGE/apps/AirTop/AirTop" "$BUILD/AirTop.rsrc"
"$TOOLS/resattr/resattr" -O -o "$STAGE/apps/AirTop/AirTop" "$BUILD/AirTop.rsrc"
ln -s ../../../../apps/AirTop/AirTop "$STAGE/data/deskbar/menu/Applications/AirTop"
cp "$ROOT/LICENSE" "$STAGE/data/licenses/MIT"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$STAGE/documentation/packages/airtop/"
cp -R "$ROOT/docs" "$STAGE/documentation/packages/airtop/"
cp "$INFO" "$STAGE/.PackageInfo"
( cd "$STAGE" && "$TOOLS/mimeset" --all -f --mimedb data/mime_db --mimedb "$MIMEDB" apps/AirTop/AirTop )
rm -f "$FILE"
"$TOOLS/package/package" create -q -C "$STAGE" "$FILE"
"$TOOLS/package/package" list -i "$FILE" | grep -E "architecture|version"
echo "$FILE"
