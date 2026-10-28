#!/bin/bash
# Makes the application bundle self-contained: copies the libraries it uses that aren't part of macOS (i.e.
# Homebrew's SDL) into Contents/Frameworks, points the binaries at the copies, and signs the bundle (ad hoc, so it
# runs on Apple Silicon after the binaries were changed).
#
# usage: bundle-dylibs.sh path/to/RALibretro.app
set -euo pipefail

app="$1"
exe="$app/Contents/MacOS/$(basename "$app" .app)"
frameworks="$app/Contents/Frameworks"
mkdir -p "$frameworks"

# the libraries a binary loads from outside of macOS and the bundle
external_libraries() {
  otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -v -E '^(/System/|/usr/lib/|@)' || true
}

bundle_library() { # source path, name in the bundle
  if [ ! -f "$frameworks/$2" ]; then
    cp -L "$1" "$frameworks/$2"
    chmod u+w "$frameworks/$2"
    install_name_tool -id "@rpath/$2" "$frameworks/$2" 2> /dev/null
  fi
}

queue=("$exe")
while ((${#queue[@]})); do
  binary="${queue[0]}"
  queue=("${queue[@]:1}")
  for library in $(external_libraries "$binary"); do
    name=$(basename "$library")
    if [ ! -f "$frameworks/$name" ]; then
      bundle_library "$library" "$name"
      queue+=("$frameworks/$name")
    fi
    install_name_tool -change "$library" "@rpath/$name" "$binary" 2> /dev/null
  done
done

# SDL2 is sdl2-compat, which loads SDL3 when it starts (from its own folder: @loader_path/libSDL3.dylib)
if [ -f "$frameworks/libSDL2-2.0.0.dylib" ] && [ ! -f "$frameworks/libSDL3.dylib" ]; then
  for sdl3 in "${SDL3_LIBRARY:-}" /opt/homebrew/opt/sdl3/lib/libSDL3.0.dylib /usr/local/opt/sdl3/lib/libSDL3.0.dylib; do
    if [ -n "$sdl3" ] && [ -f "$sdl3" ]; then
      bundle_library "$sdl3" libSDL3.dylib
      break
    fi
  done
  if [ ! -f "$frameworks/libSDL3.dylib" ]; then
    echo "warning: SDL3 not found (set SDL3_LIBRARY); the bundle needs it next to SDL2" >&2
  fi
fi

# the executable finds the bundled libraries through its run path, and only there (not i.e. in Homebrew's folder)
for rpath in $(otool -l "$exe" | awk '/LC_RPATH/ { getline; getline; print $2 }'); do
  if [[ "$rpath" != @* ]]; then
    install_name_tool -delete_rpath "$rpath" "$exe" 2> /dev/null
  fi
done
if ! otool -l "$exe" | grep -q "@executable_path/../Frameworks"; then
  install_name_tool -add_rpath "@executable_path/../Frameworks" "$exe" 2> /dev/null
fi

for library in "$frameworks"/*.dylib; do
  codesign --force --sign - "$library" 2> /dev/null
done
codesign --force --sign - "$app" 2> /dev/null
