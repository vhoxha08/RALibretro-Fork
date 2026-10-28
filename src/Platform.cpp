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
#include <SDL_opengl.h>

#include "host/Host.h"
#include "stb_image.h"

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

// Textures for the toolkit's images (badges, overlay), created with the frontend GL context current.
// The ImGui OpenGL renderer uses the texture name as its ImTextureID.
static uintptr_t createTexture(const void* pixels, int width, int height, GLenum format, GLenum type)
{
  GLint previous = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);

  GLuint texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, type, pixels);
  glBindTexture(GL_TEXTURE_2D, previous);

  // don't leave an error behind for RALibretro's GL wrapper to find
  if (glGetError() != GL_NO_ERROR)
  {
    glDeleteTextures(1, &texture);
    return 0;
  }

  return texture;
}

static uintptr_t createTextureFromFile(const char* utf8Path, int* width, int* height)
{
  int channels = 0;
  unsigned char* rgba = stbi_load(utf8Path, width, height, &channels, 4);
  if (rgba == nullptr)
    return 0;

  const uintptr_t texture = createTexture(rgba, *width, *height, GL_RGBA, GL_UNSIGNED_BYTE);
  stbi_image_free(rgba);
  return texture;
}

static uintptr_t createTextureFromPixels(const uint32_t* argb, int width, int height)
{
  // 0xAARRGGBB words are B,G,R,A bytes in memory on little endian machines
  return createTexture(argb, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV);
}

static void destroyTexture(uintptr_t texture)
{
  const GLuint name = static_cast<GLuint>(texture);
  glDeleteTextures(1, &name);
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
  RA_SetTextureHandlers(createTextureFromFile, createTextureFromPixels, destroyTexture);
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
