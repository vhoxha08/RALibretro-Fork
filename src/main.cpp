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

#define WINDOWS_IGNORE_PACKING_MISMATCH 1 // prevent "Windows headers require the default packing option" error - need to upgrade SDL library to fix it

#include <SDL.h>
#ifdef _WIN32
#include <SDL_syswm.h> // (outside Windows, it brings in X11, whose "None" breaks enums)
#endif

#include "Application.h"
#include "Util.h"

#ifndef _WIN32
#include "host/Host.h"
#endif

#include <signal.h>

extern Application app;

bool isGameActive()
{
  return app.isGameActive();
}

void getGameName(char name[], size_t len)
{
  std::string fileName = util::fileName(app.gameName());
  strncpy(name, fileName.c_str(), len);
  name[len - 1] = '\0';
}

// The toolkit calls these from its UI thread or from the game loop (RA_DoAchievementsFrame). Elsewhere than on
// Windows the game runs on its own thread, which they are handed to.
#ifdef _WIN32
#define ON_GAME_THREAD(code) code
#else
#define ON_GAME_THREAD(code) app.postToGameThread([=]() { code; })
#endif

void pauseEmulation()
{
  ON_GAME_THREAD(app.pauseGame(true));
}

void resume()
{
  ON_GAME_THREAD(app.pauseGame(false));
}

void reset()
{
  // this is called when the user switches from non-hardcore to hardcore - validate the config settings
  ON_GAME_THREAD(if (app.validateHardcoreEnablement()) app.resetGame());
}

void loadROM(const char* path)
{
  const std::string pathCopy(path);
  ON_GAME_THREAD(app.loadGame(pathCopy));
}

extern "C" void abort_handler(int signal_number)
{
  // cleaning up can itself fail and abort again; only try once
  static bool handling = false;
  if (handling)
    return;
  handling = true;

  app.logger().error("[APP] abort() called");
  app.unloadCore();
  app.destroy();
}

int main(int argc, char* argv[])
{
  signal(SIGABRT, &abort_handler);

#ifndef _WIN32
  // the native user interface (host/Host.h) must exist before the application creates its window
  if (!host::init(argc, argv))
    return 1;
#endif

  bool ok = app.init("RALibRetro", 640, 480);
#ifndef _WIN32
  // the game runs on its own thread; this one runs the user interface
  if (ok)
  {
    ok = app.runThreaded(argc, argv);
    app.destroy();
  }
#else
  ok &= app.handleArgs(argc, argv);

  if (ok)
  {
#if !defined(_MSC_VER)
    app.run();
#else
    __try
    {
      app.run();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
      app.logger().error("[APP] Unhandled exception %08X", GetExceptionCode());
    }
#endif

    app.destroy();
  }
#endif

#ifndef _WIN32
  host::shutdown();
#endif

  return ok ? 0 : 1;
}
