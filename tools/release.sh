#!/bin/bash
# Build a numbered firmware release and write firmware/manifest.json for the clock's nightly update check.
#   bash tools/release.sh        (from the project folder; needs PlatformIO)
# Then commit and push firmware/ to GitHub; clocks pick it up overnight (or via "Check for update").
set -e
cd "$(dirname "$0")/.."
REPO_RAW="https://raw.githubusercontent.com/TroyHurstCoding/PixelPressBox/main"
VERSION="$(TZ=America/Los_Angeles date +%Y.%m.%d-%H%M)"

printf '// version.h - written by tools/release.sh for each published build\n#pragma once\n#define FW_VERSION "%s"\n' \
  "$VERSION" > src/version.h
pio run
cp .pio/build/pixelpressbox/firmware.factory.bin firmware/pixelpressbox-seengreat-s3.bin
cp .pio/build/pixelpressbox/firmware.bin firmware/pixelpressbox-update.bin

BIN=firmware/pixelpressbox-update.bin
SIZE=$(wc -c < "$BIN" | tr -d ' ')
MD5=$( (md5sum "$BIN" 2>/dev/null || md5 -r "$BIN") | cut -d' ' -f1)
cat > firmware/manifest.json <<EOF
{
  "version": "$VERSION",
  "bin": "$REPO_RAW/firmware/pixelpressbox-update.bin",
  "size": $SIZE,
  "md5": "$MD5"
}
EOF
echo "Release $VERSION ($SIZE bytes, md5 $MD5)"
