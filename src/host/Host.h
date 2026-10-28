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

// The native user interface of the macOS/Linux build (wxWidgets): the main window with its menu bar and the
// OpenGL view the game is drawn in. The Windows build uses SDL and Win32 instead.
//
// Input from the window (keys, mouse, resizing, closing) is turned into SDL events and pushed on SDL's event
// queue, so the application keeps handling input as SDL events.
//
// The native user interface runs on the main thread (runMainLoop) while the game runs on its own thread. The
// window functions can be called from either thread: they are carried out on the main thread.
#ifndef _WIN32

#include <functional>
#include <string>

namespace host
{
  // Lifecycle; init must run before anything else uses the native user interface (i.e. RA_Init).
  bool init(int argc, char* argv[]);
  void shutdown();

  // Receives typed text (UTF-8). It doesn't go through SDL's queue: SDL3's SDL2 compatibility layer crashes
  // pushing text input events.
  void setTextInputHandler(std::function<void(const char*)> handler);

  // Handles the native events that are waiting (menus, window changes, the toolkit's windows). Window input is
  // pushed on SDL's queue for the caller to poll afterwards. Main thread only, before runMainLoop.
  void pumpEvents();

  // ===== threads =====

  bool isMainThread();
  // Runs fn on the main thread: right away when called from it, otherwise queued (and waited for if wait is set).
  void runOnMainThread(std::function<void()> fn, bool wait);
  // Runs the native event loop on the main thread until exitMainLoop is called (from any thread). tick runs every
  // few milliseconds, also while a menu is open or the window is being resized. SDL's events are pumped with it.
  void runMainLoop(std::function<void()> tick);
  void exitMainLoop();

  // ===== main window =====

  // Creates and shows the main window, with a client area (the game view) of width x height. x and y are the
  // window position, or INT_MIN to center it. Returns the native window (the HWND passed to the toolkit), a
  // wxWindow*.
  void* createMainWindow(const char* title, int x, int y, int width, int height);
  void destroyMainWindow();

  void getWindowPosition(int* x, int* y);
  // In points; the game view has getContentScale() pixels per point (2 on a Retina display).
  void getClientSize(int* width, int* height);
  float getContentScale();
  void setClientSize(int width, int height);
  void setTitle(const std::string& title);

  bool isFullscreen();
  void setFullscreen(bool fullscreen);
  void showCursor(bool show);

  // Whether the main window has the focus (not another application, nor one of the application's other windows).
  bool isMainWindowActive();

  // Confines the mouse to the game view and hides the cursor; movements then arrive as relative SDL mouse motion
  // (xrel/yrel), like SDL's relative mouse mode. Suspended while the main window doesn't have the focus.
  void setMouseCaptured(bool captured);
  // Whether the mouse is captured right now.
  bool isMouseCaptured();

  // ===== menu bar =====

  // Receives the items of a menu as it is (re)built.
  class MenuSink
  {
  public:
    virtual ~MenuSink() = default;
    virtual void beginMenu(const std::string& label) = 0;
    virtual void endMenu() = 0;
    virtual void item(const std::string& label, unsigned id, bool enabled = true, bool checked = false) = 0;
    virtual void separator() = 0;
  };

  enum class Menu
  {
    File,
    Settings,
    RetroAchievements,
    Help,
  };

  // Builds the top level menus when they open (so they always reflect the current state) and handles the
  // selected commands.
  class MenuProvider
  {
  public:
    virtual ~MenuProvider() = default;
    virtual void buildMenu(Menu menu, MenuSink& sink) = 0;
    virtual void onMenuCommand(unsigned id) = 0;
  };
  void setMenuProvider(MenuProvider* provider);

  // ===== services =====

  // Shows a message box parented to the main window. type takes the MB_ flags of Platform.h, the result is
  // IDOK, IDCANCEL, IDYES or IDNO.
  int messageBox(const char* text, const char* caption, unsigned type);

  void setClipboardText(const char* utf8Text);
  std::string getClipboardText();

  // ===== OpenGL =====

  // Contexts for the game view. The second one shares objects with the first. The first context created draws to
  // the game view. A context is current on one thread at a time: release it before another thread makes it current.
  void* createGLContext(void* shareWith);
  void  destroyGLContext(void* context);
  void  makeGLContextCurrent(void* context);
  void  releaseGLContext();
  void  swapBuffers();
  void  setSwapInterval(int interval);
  int   getSwapInterval();
  void* getGLProcAddress(const char* name);

  // Refresh rate of the display showing the main window (0 if unknown).
  int displayRefreshRate();
}

#endif
