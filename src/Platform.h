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

// Platform differences shared by the Windows and the macOS/Linux builds.

#ifdef _WIN32

 #define PATH_SEPARATOR      "\\"
 #define PATH_SEPARATOR_CHAR '\\'
 #define CORE_EXTENSION      ".dll"

#else

 #define PATH_SEPARATOR      "/"
 #define PATH_SEPARATOR_CHAR '/'
 #ifdef __APPLE__
  #define CORE_EXTENSION     ".dylib"
 #else
  #define CORE_EXTENSION     ".so"
 #endif

 // HWND, BYTE, DWORD, RECT, ... (opaque handle types; RAIntegration's shim when not on Windows)
 #include <wtypes.h>
 #include <stdint.h>

 typedef uint16_t WORD;
 typedef unsigned int UINT;

 // menu item states (the ImGui menu derives them from the application state)
 #define MF_ENABLED  0x0000U
 #define MF_DISABLED 0x0002U

 // MessageBox with the subset of the Win32 API RALibretro uses, implemented with SDL message boxes.
 // On these platforms g_mainWindow holds the SDL_Window*, so it can be passed as the parent.
 #define MB_OK          0x00000000U
 #define MB_OKCANCEL    0x00000001U
 #define MB_YESNO       0x00000004U
 #define MB_ICONERROR   0x00000010U
 #define MB_ICONWARNING 0x00000030U

 #define IDOK     1
 #define IDCANCEL 2
 #define IDYES    6
 #define IDNO     7

 int MessageBox(HWND hWnd, const char* text, const char* caption, unsigned int type);

 // Connects the statically linked toolkit's clipboard and sounds to SDL (call after RA_Init)
 void installHostServices();
 void shutdownHostServices();

 // Microsoft's argument order (result first)
 #include <errno.h>
 #include <time.h>
 inline int localtime_s(struct tm* result, const time_t* time)
 {
   return localtime_r(time, result) ? 0 : errno;
 }

 inline int gmtime_s(struct tm* result, const time_t* time)
 {
   return gmtime_r(time, result) ? 0 : errno;
 }

 #include <limits.h>
 #include <stdio.h>
 #include <strings.h>
 #include <unistd.h>
 #define strnicmp strncasecmp
 #define stricmp strcasecmp
 #define MAX_PATH PATH_MAX

 inline void Sleep(unsigned int milliseconds)
 {
   usleep(milliseconds * 1000);
 }

 // like the Win32 MoveFile: nonzero on success, fails if the destination exists
 inline int MoveFile(const char* existingPath, const char* newPath)
 {
   FILE* existing = fopen(newPath, "rb");
   if (existing)
   {
     fclose(existing);
     return 0;
   }

   return rename(existingPath, newPath) == 0;
 }

#endif
