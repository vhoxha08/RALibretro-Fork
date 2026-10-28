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

// Shared by the wxWidgets parts of the user interface (HostWx.cpp, DialogWx.cpp).

#include <SDL.h>

class wxKeyEvent;
class wxKeyboardState;

namespace hostwx
{
  // the SDL key for a wx key event (SDLK_UNKNOWN if there is none)
  SDL_Keycode toSdlKeycode(const wxKeyEvent& event);
  // the SDL modifiers of a key or mouse event
  Uint16 toSdlModifiers(const wxKeyboardState& state);
  // an SDL key event for a wx key event; false if the key has no SDL equivalent
  bool toSdlKeyEvent(const wxKeyEvent& event, bool pressed, SDL_Event* sdlEvent);
}
