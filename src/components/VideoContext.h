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

#include "libretro/Components.h"

#include <SDL.h>

#include <atomic>

// The OpenGL contexts of the game view: one for the frontend (and the toolkit's overlay) and one for the core
// (hardware rendered cores draw into it), sharing objects. The Windows build creates them on its SDL window; the
// macOS/Linux build on the native window's GL view (see host/Host.h).
class VideoContext : public libretro::VideoContextComponent
{
public:
#ifdef _WIN32
  bool init(libretro::LoggerComponent* logger, SDL_Window* window);
#else
  bool init(libretro::LoggerComponent* logger);
#endif
  void destroy();

  virtual void enableCoreContext(bool enable) override;
  virtual void resetCoreContext() override;
  virtual void swapBuffers() override;

  // vsync (0 = off, 1 = on) of the game view
  static void setSwapInterval(int interval);
  static int getSwapInterval();

  // OpenGL entry points for the frontend and hardware rendered cores
  static void* getProcAddress(const char* symbol);

private:
  libretro::LoggerComponent* _logger;
#ifdef _WIN32
  SDL_Window* _window;
  SDL_GLContext _raContext;
  SDL_GLContext _coreContext;
#else
  void* _raContext = nullptr;
  void* _coreContext = nullptr;
  bool _coreCurrent = false;
#endif

  static VideoContext* s_instance;
#ifndef _WIN32
  static std::atomic<int> s_swapInterval; // readable from any thread (i.e. the audio callback)
#endif
};
