#!/bin/bash
# Signs the application bundle for distribution: with a Developer ID certificate, the hardened runtime (which
# notarization needs, with the exceptions in RALibretro.entitlements) and a secure timestamp. The bundled libraries
# first, then the bundle. The build signs it ad hoc only (bundle-dylibs.sh).
#
# usage: sign.sh path/to/RALibretro.app identity
#   identity: the certificate's name ("Developer ID Application: <name> (<team ID>)"), or - to sign ad hoc (i.e. to
#   try the hardened runtime without a certificate)
set -euo pipefail
app="$1"
identity="$2"
entitlements="$(cd "$(dirname "$0")" && pwd)/RALibretro.entitlements"

timestamp=--timestamp
if [ "$identity" = - ]; then
  timestamp=--timestamp=none
fi

for library in "$app"/Contents/Frameworks/*.dylib; do
  codesign --force "$timestamp" --options runtime --sign "$identity" "$library"
done
codesign --force "$timestamp" --options runtime --entitlements "$entitlements" --sign "$identity" "$app"
codesign --verify --strict --deep --verbose=2 "$app"
