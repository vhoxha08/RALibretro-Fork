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

#include "host/Host.h"

#include <nfd.h>

#include <cstring>
#include <string>
#include <vector>

int MessageBox(HWND, const char* text, const char* caption, unsigned int type)
{
  return host::messageBox(text, caption, type);
}

// ---- host services for the statically linked toolkit ----

#include "RA_Interface_Host.h"

#include <mutex>
#include <string>

static void setClipboardText(const char* utf8Text)
{
  host::setClipboardText(utf8Text);
}

static const char* getClipboardText()
{
  static std::string text; // must stay valid until the next call
  text = host::getClipboardText();
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

static int showFileDialog(int mode, const char*, const char* filters, const char* defaultPath,
                          const char* defaultName, char* result, int resultSize)
{
  // "Description\tspec\n" entries
  std::vector<std::string> names, specs;
  for (const char* line = filters; line && *line;)
  {
    const char* tab = strchr(line, '\t');
    const char* end = strchr(line, '\n');
    if (!end)
      end = line + strlen(line);
    if (tab && tab < end)
    {
      names.emplace_back(line, tab);
      specs.emplace_back(tab + 1, end);
    }
    line = *end ? end + 1 : end;
  }

  std::vector<nfdu8filteritem_t> items;
  for (size_t i = 0; i < names.size(); ++i)
    items.push_back({names[i].c_str(), specs[i].c_str()});

  if (NFD_Init() != NFD_OKAY)
    return 0;

  const char* path = (defaultPath && *defaultPath) ? defaultPath : nullptr;
  nfdu8char_t* outPath = nullptr;
  nfdresult_t status = NFD_CANCEL;
  switch (mode)
  {
    case 1:
      status = NFD_OpenDialogU8(&outPath, items.data(), (nfdfiltersize_t)items.size(), path);
      break;
    case 2:
      status = NFD_SaveDialogU8(&outPath, items.data(), (nfdfiltersize_t)items.size(), path,
                                (defaultName && *defaultName) ? defaultName : nullptr);
      break;
    case 3:
      status = NFD_PickFolderU8(&outPath, path);
      break;
  }

  int ok = 0;
  if (status == NFD_OKAY && outPath)
  {
    if (strlen(outPath) < (size_t)resultSize)
    {
      strcpy(result, outPath);
      ok = 1;
    }
    NFD_FreePathU8(outPath);
  }

  NFD_Quit();
  return ok;
}

void installHostServices()
{
  RA_SetClipboardHandlers(setClipboardText, getClipboardText);
  RA_SetAudioHandlers(playSound, nullptr);
  RA_SetFileDialogHandler(showFileDialog);
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
