#!/bin/bash
# Makes the application icon (.icns) from a 1024x1024 image (etc/macos/icon.png, see make-icon.swift).
#
# usage: make-icns.sh source.png target.icns
set -euo pipefail

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

sips -s format png "$1" --out "$work/icon.png" > /dev/null
mkdir "$work/icon.iconset"
for size in 16 32 128 256 512; do
  sips -z $size $size "$work/icon.png" --out "$work/icon.iconset/icon_${size}x${size}.png" > /dev/null
  sips -z $((size * 2)) $((size * 2)) "$work/icon.png" --out "$work/icon.iconset/icon_${size}x${size}@2x.png" > /dev/null
done
iconutil -c icns "$work/icon.iconset" -o "$2"
