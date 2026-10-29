#!/bin/bash
# Smoke test with a game: starts RALibretro with it, takes a screenshot of the game, opens the in-game overlay
# (Escape) and moves through it, takes a screenshot of that, closes the overlay and quits. Fails when the game doesn't
# load, the application doesn't stay up, or the log has video errors (OpenGL errors, "failed to ensure view"); other
# errors are listed. The screenshots and the log go to the output folder.
#
# usage: smoke-test.sh [-a app] [-o folder] [-w seconds] [-k keys] core system game
#   -a  the application (default: /Applications/RALibretro.app; a build folder's uses that folder's Cores)
#   -o  where the screenshots and the log go (default: $TMPDIR/ralibretro-smoke-test)
#   -w  how long the game runs before the first screenshot (default: 15)
#   -k  keys pressed in the overlay, as code:count:delay (default: 125:3:0.3, Down three times; see drive-app.swift)
#
# i.e. etc/macos/smoke-test.sh desmume_libretro 18 ~/roms/nds/game.nds
#
# The terminal running it needs the Screen Recording (screenshots) and Accessibility (keys) permissions.
set -euo pipefail

app=/Applications/RALibretro.app
out="${TMPDIR:-/tmp}"
out="${out%/}/ralibretro-smoke-test"
wait=15
keys=125:3:0.3
while getopts "a:o:w:k:" option; do
  case "$option" in
    a) app="$OPTARG" ;;
    o) out="$OPTARG" ;;
    w) wait="$OPTARG" ;;
    k) keys="$OPTARG" ;;
    *) sed -n '2,15s/^# \{0,1\}//p' "$0" >&2; exit 2 ;;
  esac
done
shift $((OPTIND - 1))
if [ $# -ne 3 ]; then
  sed -n '2,15s/^# \{0,1\}//p' "$0" >&2
  exit 2
fi
core="$1"
system="$2"
game="$3"

mkdir -p "$out"
name="$core-$(basename "${game%.*}" | tr -cs 'A-Za-z0-9' '_' | sed 's/_*$//' | cut -c1-40)"
log="$out/$name.log"

# the helper is built on first use (and again when it changed)
drive="$out/drive-app"
source="$(dirname "$0")/drive-app.swift"
if [ ! -x "$drive" ] || [ "$source" -nt "$drive" ]; then
  swiftc -O "$source" -o "$drive"
fi

"$app/Contents/MacOS/$(basename "$app" .app)" -c "$core" -s "$system" -g "$game" > "$log" 2>&1 &
pid=$!

failures=()
alive() { kill -0 "$pid" 2>/dev/null; }
loaded() { grep -q '\[COR\] Content ".*" loaded' "$log"; }
screenshot() {
  "$drive" activate "$pid"
  sleep 1
  # the main window, or the whole screen when the game didn't load (the dialog saying why is a window of its own)
  local window=""
  if loaded; then
    window="$("$drive" window "$pid" 2>/dev/null)" || window=""
  fi
  if [ -n "$window" ]; then
    screencapture -x -o -l "$window" "$out/$name-$1.png"
  else
    screencapture -x "$out/$name-$1.png"
  fi
}

sleep "$wait"
if alive; then
  screenshot game
  # shellcheck disable=SC2086 # (several keys)
  "$drive" keys "$pid" 53:1:2 $keys
  sleep 1
  screenshot overlay
  "$drive" keys "$pid" 53:1:1
  alive || failures+=("exited while the overlay was shown")
else
  failures+=("exited before the screenshots (i.e. the game didn't load: see the log)")
fi

# quit like the window's close button (SIGTERM shuts down cleanly), forcibly if it doesn't
if alive; then
  kill "$pid"
  for _ in $(seq 20); do
    alive || break
    sleep 0.5
  done
  if alive; then
    kill -9 "$pid"
    failures+=("didn't quit")
  fi
fi
wait "$pid" 2>/dev/null || true

# (a game that didn't load can leave the application up, showing why)
loaded || failures+=("the game didn't load (see the screenshot and the log)")
gl_errors=$(grep -c '\[ERROR\] \[OGL\]' "$log" || true)
view_errors=$(grep -c 'failed to ensure view' "$log" || true)
[ "$gl_errors" -eq 0 ] || failures+=("$gl_errors OpenGL error lines")
[ "$view_errors" -eq 0 ] || failures+=("$view_errors \"failed to ensure view\" lines")

echo "$name: log and screenshots in $out"
other_errors=$(grep '\[ERROR\]' "$log" | grep -v '\[OGL\]\|failed to ensure view' | sort | uniq -c | sort -rn || true)
if [ -n "$other_errors" ]; then
  echo "  other errors (not failures):"
  echo "$other_errors" | head -10 | sed 's/^/    /'
fi
if [ ${#failures[@]} -eq 0 ]; then
  echo "  passed"
else
  printf '  FAILED: %s\n' "${failures[@]}"
  exit 1
fi
