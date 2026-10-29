# RALibretro

RALibretro is a multi-emulator that can be used to develop RetroAchievements and, of course, earn them.

The "multi-emulation" feature is only possible because it uses [libretro](https://github.com/libretro/) cores to do the actual emulation. What RALibretro does is to connect the emulation to the tools used to create RetroAchievements.

The integration with RetroAchievements.org site is done by the [RAIntegration](https://github.com/RetroAchievements/RAIntegration).


## macOS (`cross-platform` branch)

This branch runs RALibretro natively on macOS (Apple Silicon). The Windows build is unchanged, except for the name it
gives RetroAchievements (see below).

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

### Requirements

- macOS 11 or later on Apple Silicon, with the Xcode command line tools (`xcode-select --install`)
- [Homebrew](https://brew.sh): `brew install cmake ninja sdl2-compat`

### Building

```
git clone --recursive -b cross-platform https://github.com/vhoxha08/RALibretro.git
cd RALibretro
cmake -B build-release.noindex -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build-release.noindex RALibretro
```

The result is a self-contained `build-release.noindex/RALibretro.app` (SDL is copied into it): copy it to
`/Applications`. It's signed ad hoc, so on another Mac it has to be opened with right-click → Open the first time.
The first build takes a while (it builds wxWidgets).

(A build folder named `*.noindex` keeps Spotlight, and the Apps view, from listing the application built in it.)

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
