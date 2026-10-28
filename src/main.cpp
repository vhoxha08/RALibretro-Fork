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
#include <SDL_syswm.h>

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

void pauseEmulation()
{
  app.pauseGame(true);
}

void resume()
{
  app.pauseGame(false);
}

void reset()
{
  // this is called when the user switches from non-hardcore to hardcore - validate the config settings
  if (app.validateHardcoreEnablement())
    app.resetGame();
}

void loadROM(const char* path)
{
  app.loadGame(path);
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

#ifndef _WIN32
  host::shutdown();
#endif

  return ok ? 0 : 1;
}
