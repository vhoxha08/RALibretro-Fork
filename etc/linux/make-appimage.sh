#!/bin/bash
# Makes RALibretro-<arch>.AppImage from a build folder: installs the build into an AppDir and has linuxdeploy (with its
# GTK plugin) add the libraries it uses that a Linux system may not have; OpenGL/EGL stay the system's (its GPU
# driver). linuxdeploy and the plugin are downloaded into the build folder the first time.
#
# usage: make-appimage.sh build-folder [output-folder]
set -euo pipefail

build="$(cd "$1" && pwd)"
out="$(cd "${2:-.}" && pwd)"
arch="$(uname -m)"
tools="$build/appimage-tools"
appdir="$build/AppDir"

mkdir -p "$tools"
if [ ! -x "$tools/linuxdeploy-$arch.AppImage" ]; then
  curl -fsSL -o "$tools/linuxdeploy-$arch.AppImage" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$arch.AppImage"
  chmod +x "$tools/linuxdeploy-$arch.AppImage"
fi
if [ ! -x "$tools/linuxdeploy-plugin-gtk.sh" ]; then
  curl -fsSL -o "$tools/linuxdeploy-plugin-gtk.sh" \
    "https://raw.githubusercontent.com/linuxdeploy/linuxdeploy-plugin-gtk/master/linuxdeploy-plugin-gtk.sh"
  chmod +x "$tools/linuxdeploy-plugin-gtk.sh"
fi

rm -rf "$appdir"
DESTDIR="$appdir" cmake --install "$build" --prefix /usr

# sdl2-compat (i.e. Fedora's SDL2) loads SDL3 when it starts, which linuxdeploy can't see
extra=()
sdl3="$(ldconfig -p | awk '/libSDL3\.so\.0 / { print $NF; exit }')"
if [ -n "$sdl3" ]; then
  extra+=(--library "$sdl3")
fi

# no FUSE in containers (i.e. a distrobox, or CI): linuxdeploy runs extracted
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$tools:$PATH" # where linuxdeploy finds the plugin
export DEPLOY_GTK_VERSION=3
export LDAI_OUTPUT="$out/RALibretro-$arch.AppImage"
cd "$out"
"$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" --plugin gtk "${extra[@]}" \
  --desktop-file "$appdir/usr/share/applications/RALibretro.desktop" \
  --icon-file "$appdir/usr/share/icons/hicolor/256x256/apps/ralibretro.png"

# The GTK plugin makes GTK use X11 (through XWayland under Wayland). Under X11, wxGLCanvas draws with EGL, which can't
# make a surface for the window with NVIDIA's driver (the game stays black): use Wayland when there is one.
hook="$appdir/apprun-hooks/linuxdeploy-plugin-gtk.sh"
grep -q '^export GDK_BACKEND=x11' "$hook"
sed -i 's/^export GDK_BACKEND=x11.*/export GDK_BACKEND="${GDK_BACKEND:-wayland,x11}" # Wayland when there is one (see make-appimage.sh)/' "$hook"
# It also sets GTK_THEME (Adwaita, light or dark as the desktop was when RALibretro started), which makes GTK ignore the
# desktop's theme and its changes: GTK follows the desktop's theme (i.e. KDE's Breeze) without it.
grep -q '^export GTK_THEME=' "$hook"
sed -i '/^export GTK_THEME=/d' "$hook"

"$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" --output appimage
echo "made $LDAI_OUTPUT"
