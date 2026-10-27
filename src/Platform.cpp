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

#include <cstring>

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

// ---- host services for the statically linked toolkit ----

#include "RA_Interface_Host.h"

#include <mutex>
#include <string>

static void setClipboardText(const char* utf8Text)
{
  SDL_SetClipboardText(utf8Text);
}

static const char* getClipboardText()
{
  static std::string text; // must stay valid until the next call
  char* clipboard = SDL_GetClipboardText();
  text = clipboard ? clipboard : "";
  SDL_free(clipboard);
  return text.c_str();
}

// Sounds go through their own SDL audio device so they don't disturb the core's audio stream.
// The toolkit can request a sound from any thread.
static std::mutex g_soundMutex;
static SDL_AudioDeviceID g_soundDevice = 0;
static SDL_AudioSpec g_soundSpec;

static void playSound(const char* utf8Path)
{
  std::lock_guard<std::mutex> lock(g_soundMutex);

  if (g_soundDevice == 0)
  {
    SDL_AudioSpec want = {};
    want.freq = 44100;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    g_soundDevice = SDL_OpenAudioDevice(NULL, 0, &want, &g_soundSpec, 0);
    if (g_soundDevice == 0)
      return;

    SDL_PauseAudioDevice(g_soundDevice, 0);
  }

  SDL_AudioSpec wavSpec;
  Uint8* wavData = nullptr;
  Uint32 wavLength = 0;
  if (SDL_LoadWAV(utf8Path, &wavSpec, &wavData, &wavLength) == nullptr)
    return;

  SDL_AudioCVT cvt;
  if (SDL_BuildAudioCVT(&cvt, wavSpec.format, wavSpec.channels, wavSpec.freq,
                        g_soundSpec.format, g_soundSpec.channels, g_soundSpec.freq) >= 0)
  {
    std::string buffer(wavLength * (cvt.len_mult > 0 ? cvt.len_mult : 1), '\0');
    memcpy(&buffer[0], wavData, wavLength);
    cvt.buf = reinterpret_cast<Uint8*>(&buffer[0]);
    cvt.len = static_cast<int>(wavLength);

    if (cvt.needed == 0 || SDL_ConvertAudio(&cvt) == 0)
      SDL_QueueAudio(g_soundDevice, cvt.buf, cvt.needed ? static_cast<Uint32>(cvt.len_cvt) : wavLength);
  }

  SDL_FreeWAV(wavData);
}

void installHostServices()
{
  RA_SetClipboardHandlers(setClipboardText, getClipboardText);
  RA_SetAudioHandlers(playSound, nullptr);
}

void shutdownHostServices()
{
  std::lock_guard<std::mutex> lock(g_soundMutex);
  if (g_soundDevice != 0)
  {
    SDL_CloseAudioDevice(g_soundDevice);
    g_soundDevice = 0;
  }
}

#endif
