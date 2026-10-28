/*
Copyright (C) 2018 Andre Leiradella

This file is part of RALibretro.

RALibretro is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

RALibretro is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with RALibretro.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL.h>
#include "Fsm.h"

#include <RA_Interface.h>
#include "Platform.h"

#include "components/Allocator.h"
#include "components/Audio.h"
#include "components/Config.h"
#include "components/Input.h"
#include "components/Logger.h"
#include "components/Microphone.h"
#include "components/VideoContext.h"
#include "components/Video.h"

#include "Emulator.h"
#include "KeyBinds.h"
#include "Memory.h"
#include "States.h"

#ifndef _WIN32
#include "host/Host.h"
#endif

class Application
#ifndef _WIN32
  : public host::MenuProvider
#endif
{
public:
  Application();

  // Lifecycle
  bool init(const char* title, int width, int height);
  bool handleArgs(int argc, char *argv[]);

  void run();
  void destroy();

#ifndef _WIN32
  // Runs the game (handleArgs, then run) on its own thread while the calling (main) thread runs the native user
  // interface, until the application quits. Returns false if the command line couldn't be handled.
  bool runThreaded(int argc, char* argv[]);
  // Runs fn on the game thread: right away when called from it, otherwise from the game loop.
  void postToGameThread(std::function<void()> fn);
#endif

  // FSM
  bool loadCore(const std::string& coreName);
  void updateMenu();
  bool loadGame(const std::string& path);
  void unloadCore();
  void resetGame();
  bool hardcore();
  bool unloadGame();
  void pauseGame(bool pause);
  bool isPaused() const;

  void printf(const char* fmt, ...);
  Logger& logger() { return _logger; }

  // RA_Integration
  bool isGameActive();
  const std::string& gameName() const { return _gameFileName; }
  bool validateHardcoreEnablement();

  bool isFullscreen() const;
  void onRotationChanged(Video::Rotation oldRotation, Video::Rotation newRotation);

  void refreshMemoryMap();

  Config& config() { return _config; }

protected:
  struct RecentItem
  {
    std::string path;
    std::string coreName;
    int system;
  };

  // Called by SDL from the audio thread
  static void s_audioCallback(void* udata, Uint8* stream, int len);

  // Helpers
  void        processEvents();
  bool        ignoreControllerInput();
  void        runSmoothed();
  void        runTurbo();
  void        pauseForBadPerformance();

  void        loadGame();
  void        enableItems(const UINT* items, size_t count, UINT enable);
  void        enableSlots();
  void        enableRecent();
  void        updateDiscMenu(bool updateLabels);
  std::string getStatePath(unsigned ndx);
  std::string getConfigPath();
  std::string getCoreConfigPath(const std::string& coreName);
  std::string getScreenshotPath();
  void        saveState(const std::string& path);
  void        saveState(unsigned ndx);
  void        saveState();
  void        loadState(const std::string& path);
  void        loadState(unsigned ndx);
  void        loadState();
  void        changeCurrentState(unsigned ndx);
  void        screenshot();
  void        aboutDialog();
  void        resizeWindow(unsigned multiplier);
  void        resizeWindow(int width, int height);
  void        toggleFullscreen();
#ifdef _WIN32
  void        handle(const SDL_SysWMEvent* syswm);
#endif
  void        handleCommand(unsigned cmd);
  bool        isMenuCommandEnabled(unsigned id);
#ifndef _WIN32
  // host::MenuProvider: the native menu bar (MenuNative.cpp)
  void        buildMenu(host::Menu menu, host::MenuSink& sink) override;
  void        onMenuCommand(unsigned id) override;
  void        addSystemsMenu(host::MenuSink& sink);
  void        addDiscMenu(host::MenuSink& sink);
  void        menuItem(host::MenuSink& sink, const std::string& label, unsigned id, bool enabled = true,
                       bool checked = false);
  void        buildMenuItems(host::Menu menu, host::MenuSink& sink);
  void        runPendingCommand();

  // work the audio callback asks the game loop to do (GL belongs to the game thread)
  void        applyAudioRequests();
  std::atomic<int>  _swapIntervalRequest{-1};
  std::atomic<bool> _pauseForBadPerformanceRequest{false};
#endif
  void        handle(const SDL_WindowEvent* window);
  void        handle(const SDL_MouseMotionEvent* motion);
  void        handle(const SDL_MouseButtonEvent* button);
  void        handle(const KeyBinds::Action action, unsigned extra);
  void        buildSystemsMenu();
  void        loadConfiguration(int* window_x, int* window_y, int* window_width, int* window_height);
  void        saveConfiguration();
  std::string serializeRecentList();
  void        updateMouseCapture();
  void        updateSpeedIndicator();
  void        toggleFastForwarding(unsigned extra);
  void        toggleBackgroundInput();
  void        setBackgroundInput(bool enabled);
  void        toggleTray();
  void        readyNextDisc(int offset);
  void        readyDisc(unsigned newDiscIndex);
  std::string getDiscLabel(unsigned index) const;

  Fsm _fsm;
  bool lastHardcore;
  bool cancelLoad;

  std::string _coreName;
  int         _system;

  SDL_Window*       _window;
  SDL_AudioSpec     _audioSpec;
  SDL_AudioDeviceID _audioDev;

  Fifo         _fifo;
  Logger       _logger;
  Config       _config;
  VideoContext _videoContext;
  Video        _video;
  Audio        _audio;
  Microphone   _microphone;
  Input        _input;
  Memory       _memory;
  States       _states;

  int          _numAudioFaults;
  int          _numAudioRecoveries;
  int          _audioGeneratedDuringFastForward;
  bool         _vsyncDisabledByAudioFaults;
  std::atomic<bool> _processingEvents{false};

  KeyBinds _keybinds;
  std::vector<RecentItem> _recentList;
  std::vector<std::string> _discPaths;
  bool _isDriveFloppy;

  Allocator<256 * 1024> _allocator;

  libretro::Components _components;
  libretro::Core       _core;

  std::string _gamePath;
  std::string _gameFileName;
  void*       _gameData;
  unsigned    _validSlots;
  bool        _gamePathIsTemporary;

  HMENU _menu;
  HMENU _cdRomMenu;
#ifndef _WIN32
  bool _fastForwardChecked = false; // the Windows build keeps this in the Turbo menu item

  // ===== threads (see runThreaded) =====

  bool isGameThread() const { return std::this_thread::get_id() == _gameThreadId; }

  std::thread::id _gameThreadId = std::this_thread::get_id();
  std::mutex _gameThreadMutex;
  std::deque<std::function<void()>> _gameTasks;

  // The menus, as built on the game thread (which owns the state they show) for the main thread to show without
  // waiting for it: macOS asks for them before handling keys (looking for shortcuts), even while the game thread
  // waits for a dialog. Published after each command and a few times per second.
  struct MenuEntry
  {
    enum class Kind { Begin, End, Item, Separator } kind;
    std::string label;
    unsigned id;
    bool enabled;
    bool checked;
  };
  void publishMenus();
  std::mutex _menusMutex;
  std::map<host::Menu, std::vector<MenuEntry>> _menus;
  std::chrono::steady_clock::time_point _menusPublished;
  bool _menusChanged = true;
#endif

  int _absViewMouseX;
  int _absViewMouseY;
};
