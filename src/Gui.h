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

// Dear ImGui user interface for the macOS/Linux build: the menu bar (in place of the Win32 menu) and
// modal dialogs. Drawn on top of the game image right before each buffer swap.
#ifndef _WIN32

#include <SDL.h>

#include <functional>

namespace gui
{
  // glContext is the frontend's (not the core's) OpenGL context; it must be current.
  bool init(SDL_Window* window, SDL_GLContext glContext);
  void destroy();

  // Draws the menu bar contents (between BeginMainMenuBar/EndMainMenuBar).
  void setMenuRenderer(std::function<void()> renderer);
  // Redraws the window (game image + GUI). Used while a modal dialog runs its own loop.
  void setRedraw(std::function<void()> redraw);

  // Returns true if ImGui uses the event (i.e. typing into a field or clicking a menu), so the game ignores it.
  bool processEvent(const SDL_Event& event);

  // Builds and renders one GUI frame. Called with the frontend GL context current, before the swap.
  void render();

  // Height of the menu bar in window coordinates (0 when hidden, i.e. in fullscreen).
  int  menuBarHeight();
  void setMenuBarVisible(bool visible);

  // Runs a modal window until draw returns false. draw is called between Begin/End of the window; onEvent
  // sees every SDL event first and returns true to consume it. Returns false if the application is quitting.
  // afterFrame runs once each frame has been drawn (outside the ImGui frame, so it may open another modal)
  // and returns false to close the window.
  bool runModal(const char* title, const std::function<bool()>& draw,
                const std::function<bool(const SDL_Event&)>& onEvent = nullptr,
                const std::function<bool()>& afterFrame = nullptr);
  bool isModalActive();
}

#endif
