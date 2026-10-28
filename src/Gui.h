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

// Dear ImGui drawing for the macOS/Linux build: the toolkit's overlay (popups, the in-game overlay), drawn on top of
// the game image right before each buffer swap. The menu bar and the dialogs are native (host/Host.h, Dialog.h).
#ifndef _WIN32

#include <SDL.h>

#include <functional>

namespace gui
{
  // The frontend's (not the core's) OpenGL context must be current. getSize returns the size of the game view.
  bool init(std::function<void(int*, int*)> getSize);
  void destroy();

  // Returns true if ImGui uses the event, so the game ignores it.
  bool processEvent(const SDL_Event& event);

  // Text typed into the window (UTF-8). Can be called from any thread.
  void addTextInput(const char* utf8);

  // Draws another component's windows (the toolkit's overlay) every frame.
  void setToolkitRenderer(std::function<void()> renderer);

  // Builds and renders one GUI frame. Called with the frontend GL context current, before the swap.
  void render();
}

#endif
