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

#include "Platform.h"

#ifndef _WIN32

#include <SDL.h>

int MessageBox(HWND hWnd, const char* text, const char* caption, unsigned int type)
{
  SDL_MessageBoxData data = {};
  data.window = static_cast<SDL_Window*>(hWnd);
  data.title = caption;
  data.message = text;

  if ((type & MB_ICONWARNING) == MB_ICONWARNING)
    data.flags = SDL_MESSAGEBOX_WARNING;
  else if (type & MB_ICONERROR)
    data.flags = SDL_MESSAGEBOX_ERROR;
  else
    data.flags = SDL_MESSAGEBOX_INFORMATION;

  // SDL lists buttons in the order given; mark the defaults for Return and Escape
  SDL_MessageBoxButtonData buttons[2] = {};
  if (type & MB_YESNO)
  {
    buttons[0] = {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDNO, "No"};
    buttons[1] = {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDYES, "Yes"};
    data.numbuttons = 2;
  }
  else if (type & MB_OKCANCEL)
  {
    buttons[0] = {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDCANCEL, "Cancel"};
    buttons[1] = {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, IDOK, "OK"};
    data.numbuttons = 2;
  }
  else
  {
    buttons[0] = {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, IDOK, "OK"};
    data.numbuttons = 1;
  }
  data.buttons = buttons;

  int buttonId = 0;
  if (SDL_ShowMessageBox(&data, &buttonId) != 0 || buttonId < 0)
    return (type & MB_YESNO) ? IDNO : IDCANCEL;

  return buttonId;
}

#endif
