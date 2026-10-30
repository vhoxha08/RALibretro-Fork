# RALibretro

RALibretro is a multi-emulator that can be used to develop RetroAchievements and, of course, earn them.

The "multi-emulation" feature is only possible because it uses [libretro](https://github.com/libretro/) cores to do the actual emulation. What RALibretro does is to connect the emulation to the tools used to create RetroAchievements.

The integration with RetroAchievements.org site is done by the [RAIntegration](https://github.com/RetroAchievements/RAIntegration).


## macOS and Linux (`cross-platform` branch)

This branch runs RALibretro natively on macOS (Apple Silicon) and Linux (x86_64). The Windows build is unchanged,
except for the name it gives RetroAchievements (see below).

It isn't the official RALibretro: it tells RetroAchievements it's `RALibRetro-fork` (in its User-Agent). The server
doesn't know that client, so it accepts casual unlocks only (a "Warning: Unknown Emulator" entry says so) until the
RetroAchievements team approves it for hardcore. The toolkit's settings and login are kept in
`RAPrefs_RALibRetro-fork.cfg`, copied from RALibretro's `RAPrefs_RALibRetro.cfg` the first time.

- Native window, menu bar and dialogs (wxWidgets), and the RetroAchievements toolkit's windows (Memory Inspector,
  Achievement Editor, ...) as native windows too.
- The game runs on its own thread: menus, resizing and the toolkit's windows don't stop it.
- The toolkit's overlay (popups, scoreboards, the in-game overlay) is drawn over the game, at the display's resolution.
- [RAIntegration](https://github.com/vhoxha08/RAIntegration/tree/cross-platform) is linked into the application
  (it's a submodule, `src/RAIntegration`) instead of being loaded from `RA_Integration.dll`.

### Download

Each push to the `cross-platform` branch is built by GitHub Actions and published as a
[release](https://github.com/vhoxha08/RALibretro-Fork/releases). The latest:

- macOS 15 or later on Apple Silicon:
  [RALibretro-macOS.zip](https://github.com/vhoxha08/RALibretro-Fork/releases/latest/download/RALibretro-macOS.zip).
  Unzip it and copy `RALibretro.app` to `/Applications`. The application is signed ad hoc, not by an Apple developer,
  so macOS refuses to open it the first time: open it, then allow it in *System Settings → Privacy & Security*
  (*Open Anyway*).
- Linux on x86_64 (glibc 2.35 or later: Ubuntu 22.04, Fedora 36 and newer, Bazzite):
  [RALibretro-x86_64.AppImage](https://github.com/vhoxha08/RALibretro-Fork/releases/latest/download/RALibretro-x86_64.AppImage).
  Make it executable (`chmod +x RALibretro-x86_64.AppImage`) and run it.
- Windows x64: [RALibretro-Windows-x64.zip](https://github.com/vhoxha08/RALibretro-Fork/releases/latest/download/RALibretro-Windows-x64.zip).
  Unzip it and run `RALibretro\RALibRetro.exe`. Like the official RALibretro, it downloads the official
  `RA_Integration.dll` when it starts. (Building it yourself, from `RALibretro.sln`: the version is the closest tag, so
  delete the releases' `build-...` tags from your clone first, i.e. `git tag -l "build-*" | xargs git tag -d`.)

### macOS: requirements

- macOS 11 or later on Apple Silicon, with the Xcode command line tools (`xcode-select --install`)
- [Homebrew](https://brew.sh): `brew install cmake ninja sdl2-compat`

### macOS: building

```
git clone --recursive -b cross-platform https://github.com/vhoxha08/RALibretro-Fork.git RALibretro
cd RALibretro
cmake -B build-release.noindex -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-release.noindex RALibretro
```

The result is a self-contained `build-release.noindex/RALibretro.app` (SDL is copied into it): copy it to
`/Applications`. It's signed ad hoc, so on another Mac it has to be allowed the first time (see above), and it needs
the version of macOS it was built on or later (Homebrew's SDL is built for it; `-DCMAKE_OSX_DEPLOYMENT_TARGET=...`
sets it for the rest). The first build takes a while (it builds wxWidgets).

(A build folder named `*.noindex` keeps Spotlight, and the Apps view, from listing the application built in it.)

### Linux: building

In a Fedora or Ubuntu system (or a distrobox, on an immutable one like Bazzite or SteamOS), with GTK 3, SDL 2,
libcurl, OpenGL/EGL, X11 and Wayland's development packages, i.e. on Ubuntu:

```
sudo apt install build-essential cmake ninja-build git pkg-config libgtk-3-dev libsdl2-dev libcurl4-openssl-dev \
  libgl-dev libegl-dev libx11-dev libwayland-dev wayland-protocols libxkbcommon-dev zlib1g-dev
git clone --recursive -b cross-platform https://github.com/vhoxha08/RALibretro-Fork.git RALibretro
cd RALibretro
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-release RALibretro
etc/linux/make-appimage.sh build-release   # RALibretro-x86_64.AppImage (downloads linuxdeploy the first time)
```

The AppImage uses Wayland when there is one, X11 otherwise (both work with NVIDIA's driver). The data goes to
`~/.local/share/RALibretro`, like
`~/Library/Application Support/RALibretro` on macOS (below).

### Where the data goes

- `~/Library/Application Support/RALibretro`: cores, settings, saves, screenshots, system files (BIOS) and the
  toolkit's data (`RACache`, the login). The application installs its cores list and overlay files there.
- *Portable*: when a `Cores` folder is next to the application (or next to the executable, i.e. in a build folder),
  that folder is used instead, like the Windows version.

Cores are downloaded with *Settings → Manage Cores*.

### Working on RAIntegration at the same time

CMake uses the submodule; without it, a RAIntegration checkout next to this one (`../RAIntegration`). Another one can
be picked with `-DRA_INTEGRATION_DIR=/path/to/RAIntegration` (it needs its submodules:
`git submodule update --init --recursive`). To build with a newer RAIntegration:

```
git submodule update --remote src/RAIntegration
git add src/RAIntegration && git commit -m "Update RAIntegration"
```

### Smoke test

`etc/macos/smoke-test.sh` starts a game with a core, takes screenshots of the game and of the in-game overlay, and
quits. It fails when the game doesn't load, the application doesn't stay up, or the log has video errors:

```
etc/macos/smoke-test.sh desmume_libretro 18 ~/roms/nds/game.nds
etc/macos/smoke-test.sh -a build.noindex/RALibretro.app quicknes_libretro 7 ~/roms/nes/game.nes
```

The second argument is the RetroAchievements console id (i.e. 7 NES, 18 Nintendo DS: a core's `systems` in
`etc/cores.json`). The screenshots and the log go to `$TMPDIR/ralibretro-smoke-test` (`-o` picks another folder). The
terminal needs the Screen Recording and Accessibility permissions.


## Building RALibretro with MSYS2/Makefile

### Install MSYS2

1. Go to [http://www.msys2.org/](http://www.msys2.org/) and download the 32 bit version.
2. Follow the installation instructions on the site [http://www.msys2.org/](http://www.msys2.org/).
3. When on the prompt for the first time, run `pacman -Syu` (**NOTE**: at the end of this command it will ask you to close the terminal window **without** going back to the prompt.)
4. Launch the MSYS2 terminal again and run `pacman -Su`

### Install the toolchain

```
$ pacman -S make git zip mingw-w64-i686-gcc mingw-w64-i686-SDL2 mingw-w64-i686-gcc-libs
```

### Clone the repo

```
$ git clone --recursive --depth 1 https://github.com/RetroAchievements/RALibretro.git
```

### Build

```
$ cd RALibretro
$ make
```

**NOTE**: use `make` for a release build or `make DEBUG=1` for a debug build. Don't forget to run `make clean` first if switching between a release build and a debug build.

## Building RALibretro with Visual Studio

### Clone the repo

```
> git clone --recursive --depth 1 https://github.com/RetroAchievements/RALibretro.git
```

### Build

Load `RALibretro.sln` in Visual Studio and build it.

## Command Line Arguments

Argument|Description
-|-
-c [--core]|the core's name, e.g. `--core picodrive_libretro`
-s [--system]|the system id, see ConsoleID in [RAInterface/RA_Consoles.h](https://github.com/RetroAchievements/RAInterface/blob/master/RA_Consoles.h), e.g. `--system 1`
-g [--game]|full path to the game's file, e.g. `--game "C:\ROMS\GEN\Demons Of Asteborg Demo.gen"`

In order to run a game on startup, provide the core, system and game, e.g.:

`RALibretro.exe --core picodrive_libretro --system 1 --game "C:\ROMS\GEN\Demons Of Asteborg Demo.gen"`
