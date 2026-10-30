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

# Smaller: the executable without its symbols, and GTK without an SVG loader (RALibretro's images are PNG). librsvg and
# the libraries only it uses (i.e. ICU's data, 28 MB) were about half of the AppDir.
before="$(du -sm "$appdir" | cut -f1)"
strip --strip-unneeded "$appdir/usr/bin/RALibretro"

lib="$appdir/usr/lib"
needed() { { readelf -d "$@" 2>/dev/null || true; } | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | sort -u; }
soname() { { readelf -d "$1" 2>/dev/null || true; } | sed -n 's/.*Library soname: \[\(.*\)\]/\1/p'; }

# the AppDir's libraries librsvg uses, directly or not: removed below when nothing else uses them
candidates=()
queue=($(cd "$lib" && ls librsvg-2.so* 2>/dev/null))
while [ ${#queue[@]} -gt 0 ]; do
  name="${queue[0]}"
  queue=("${queue[@]:1}")
  case " ${candidates[*]} " in *" $name "*) continue ;; esac
  [ -e "$lib/$name" ] || continue
  candidates+=("$name")
  queue+=($(needed "$lib/$name"))
done

# the SVG loader (and GTK's list of loaders without it), librsvg, then what only they used
find "$lib/gdk-pixbuf-2.0" -name '*svg*' -delete
cache="$(find "$lib/gdk-pixbuf-2.0" -name loaders.cache)"
awk 'BEGIN { RS = ""; ORS = "\n\n" } !/pixbufloader.svg/' "$cache" > "$cache.new" && mv "$cache.new" "$cache"
rm -f "$lib"/librsvg-2.so* "$lib"/girepository-1.0/Rsvg-*
removed=()
changed=1
while [ $changed -eq 1 ]; do
  changed=0
  # (readelf fails on a file named .so that isn't a library: it just adds nothing)
  used="$(find "$appdir" -type f \( -name '*.so*' -o -path '*/usr/bin/*' \) -print0 |
          { xargs -0 readelf -d 2>/dev/null || true; } | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p' | sort -u)"
  for name in "${candidates[@]}"; do
    [ -e "$lib/$name" ] || continue
    so="$(soname "$lib/$name")"
    if ! grep -qxF "${so:-$name}" <<< "$used"; then
      rm -f "$lib/$name"
      removed+=("$name")
      changed=1
    fi
  done
done

# Without an SVG loader, GTK can't show an icon theme's SVG icons (i.e. all of KDE's Breeze), like the menus' arrows:
# Adwaita's icons as PNG (the symbolic ones made from its SVG ones), the icon theme RALibretro then uses (see
# useIconsWithoutSvg in src/host/HostWx.cpp)
adwaita=/usr/share/icons/Adwaita
theme="$appdir/usr/share/icons/RALibretro"
# 16 and 24 pixels, and twice that for displays scaled 200% (i.e. 4K ones): <size>@2
for size in 16 24; do
  for scale in 1 2; do
    pixels="$((size * scale))x$((size * scale))"
    name="${size}x${size}"
    [ $scale -eq 1 ] || name="$name@$scale"
    for svg in "$adwaita"/scalable/*/*-symbolic.svg; do
      dir="$theme/$name/$(basename "$(dirname "$svg")")"
      mkdir -p "$dir"
      gtk-encode-symbolic-svg -o "$dir" "$svg" "$pixels" || echo "not encoded: $svg" >&2
    done
    # the colored ones (i.e. the file dialogs' folders) are PNG already
    (cd "$adwaita/$pixels" && find . -name '*.png' ! -name '*.symbolic.png') | while read -r png; do
      mkdir -p "$theme/$name/$(dirname "$png")"
      cp "$adwaita/$pixels/$png" "$theme/$name/$png"
    done
  done
done
dirs="$(cd "$theme" && find . -mindepth 2 -maxdepth 2 -type d | sed 's|^\./||' | sort)"
{
  printf '[Icon Theme]\nName=RALibretro\nInherits=hicolor\nDirectories=%s\n' "$(echo $dirs | tr ' ' ',')"
  for dir in $dirs; do
    case "$dir" in *@2/*) scale=2 ;; *) scale=1 ;; esac
    printf '\n[%s]\nSize=%s\nScale=%s\nType=Fixed\n' "$dir" "${dir%%x*}" "$scale"
  done
} > "$theme/index.theme"

summary="AppDir: ${before} MB -> $(du -sm "$appdir" | cut -f1) MB; removed librsvg and ${removed[*]:-nothing else}"
echo "$summary"
if [ -n "${GITHUB_ACTIONS:-}" ]; then
  echo "::notice title=AppImage::$summary"
fi

"$tools/linuxdeploy-$arch.AppImage" --appdir "$appdir" --output appimage
echo "made $LDAI_OUTPUT"
