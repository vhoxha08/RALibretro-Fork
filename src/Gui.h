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

// Dear ImGui user interface for the macOS/Linux build: modal dialogs (the menu bar is native, see host/Host.h).
// Drawn on top of the game image right before each buffer swap.
#ifndef _WIN32

#include <SDL.h>

#include <functional>

namespace gui
{
  // The frontend's (not the core's) OpenGL context must be current. getSize returns the size of the game view.
  bool init(std::function<void(int*, int*)> getSize);
  void destroy();

  // Handles the native window's events; called by the modal loops before polling SDL's queue.
  void setNativeEventPump(std::function<void()> pump);
  // Redraws the window (game image + GUI). Used while a modal dialog runs its own loop.
  void setRedraw(std::function<void()> redraw);

  // Returns true if ImGui uses the event (i.e. typing into a field or clicking a menu), so the game ignores it.
  bool processEvent(const SDL_Event& event);

  // Builds and renders one GUI frame. Called with the frontend GL context current, before the swap.
  void render();

  // Runs a modal window until draw returns false. draw is called between Begin/End of the window; onEvent
  // sees every SDL event first and returns true to consume it. Returns false if the application is quitting.
  // afterFrame runs once each frame has been drawn (outside the ImGui frame, so it may open another modal)
  // and returns false to close the window.
  bool runModal(const char* title, const std::function<bool()>& draw,
                const std::function<bool(const SDL_Event&)>& onEvent = nullptr,
                const std::function<bool()>& afterFrame = nullptr);
  bool isModalActive();

  // Draws another component's windows (the toolkit's) every frame.
  void setToolkitRenderer(std::function<void()> renderer);
  // Runs once per iteration of every modal loop, after the frame (i.e. the toolkit's UI thread work).
  void setPerFrame(std::function<void()> perFrame);
  // Blocks like runModal for a window drawn by the toolkit renderer, until isOpen returns false.
  // Returns false if the application is quitting.
  bool runModalLoop(const std::function<bool()>& isOpen);
}

#endif
