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

// Host integration that only exists when RAIntegration is linked statically (macOS/Linux).
// On Windows the toolkit DLL owns a Win32 UI thread and does these things itself.
#ifndef _WIN32

#include <stdint.h>

// Directory for RACache, Overlay and RAPrefs. Call before RA_Init.
void RA_SetBaseDirectory(const char* utf8Path);

// Runs work queued for the UI thread by the toolkit's background threads. Call once per frame.
void RA_ProcessUIThreadActions(void);

// Clipboard access. getText returns UTF-8 text that stays valid until the next call.
void RA_SetClipboardHandlers(void (*setText)(const char* utf8Text), const char* (*getText)(void));

// Sound playback. playFile receives the absolute path of a .wav file.
void RA_SetAudioHandlers(void (*playFile)(const char* utf8Path), void (*beep)(void));

// Draws the toolkit's windows; call inside the Dear ImGui frame.
void RA_RenderImGui(void);

// How the toolkit's modal windows block: runModalLoop must pump events and redraw until isOpen(data) returns 0.
void RA_SetModalLoopHandler(void (*runModalLoop)(int (*isOpen)(void*), void* data));

// Textures for the toolkit's images: ImTextureIDs of the host's ImGui renderer (0 = failure). Pixels are ARGB.
void RA_SetTextureHandlers(uintptr_t (*createFromFile)(const char* utf8Path, int* width, int* height),
                           uintptr_t (*createFromPixels)(const uint32_t* argb, int width, int height),
                           void (*destroy)(uintptr_t texture));

#endif
