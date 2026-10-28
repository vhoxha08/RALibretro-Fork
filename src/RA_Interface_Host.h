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

// Receives the toolkit's overlay to draw over the game: a picture the size of the game area at the display's
// resolution (the canvas), of which the rectangle at (x, y) changed, as premultiplied ARGB pixels (rows stride pixels
// apart) starting with its top left one; NULL when there's nothing to show. Called on the UI thread.
void RA_SetOverlayImageHandler(void (*handleImage)(const uint32_t* argb, int stride, int canvasWidth, int canvasHeight,
                                                   int x, int y, int width, int height));

// The platform's file dialog. mode: 1 = open, 2 = save, 3 = select folder. filters is a list of "Description\tspec\n"
// entries (spec = "ext1,ext2"). Writes the chosen UTF-8 path to result and returns non-zero, or 0 if canceled.
void RA_SetFileDialogHandler(int (*showFileDialog)(int mode, const char* title, const char* filters,
                                                   const char* defaultPath, const char* defaultName,
                                                   char* result, int resultSize));

#endif
