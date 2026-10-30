/*
Copyright (C) 2026 RALibretro contributors

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

#include "libretro/Components.h"

#include <stdint.h>
#include <string>
#include <vector>

// A PINE server (https://github.com/GovanifY/pine, as in PCSX2): local programs (i.e. scripts, trackers) read and
// write the game's memory, and save and load states. A Unix socket ($XDG_RUNTIME_DIR, or $TMPDIR on macOS, then
// /tmp: ralibretro.sock, ralibretro.sock.<slot> for another slot than the default one), or a TCP port on 127.0.0.1
// on Windows (the slot). The requests are handled when the application polls it (between frames, on the game
// thread).
class Pine
{
public:
  static const unsigned kDefaultSlot = 28013; // PCSX2's and DuckStation's is 28011, RPCS3's 28012

  enum class Status : uint32_t
  {
    Running = 0,
    Paused = 1,
    Shutdown = 2 // no game
  };

  // What the requests act on
  class Target
  {
  public:
    virtual ~Target() {}

    // RetroAchievements addresses; false if any of the bytes isn't mapped
    virtual bool read(uint32_t address, uint8_t* buffer, uint32_t size) = 0;
    virtual bool write(uint32_t address, const uint8_t* buffer, uint32_t size) = 0;

    virtual std::string version() = 0;
    virtual std::string title() = 0;
    virtual std::string id() = 0;
    virtual std::string uuid() = 0;
    virtual std::string gameVersion() = 0;
    virtual Status status() = 0;

    virtual bool saveState(uint8_t slot) = 0;
    virtual bool loadState(uint8_t slot) = 0;
  };

  Pine();
  ~Pine();

  bool start(unsigned slot, libretro::LoggerComponent* logger);
  void stop();

  bool isRunning() const { return _listener != kNoSocket; }
  unsigned slot() const { return _slot; }

  // Accepts the clients, and answers the requests they sent completely
  void poll(Target& target);

protected:
  static const intptr_t kNoSocket = -1;

  struct Client
  {
    intptr_t socket;
    std::vector<uint8_t> input;
  };

  bool handle(Target& target, const uint8_t* request, uint32_t size, std::vector<uint8_t>& reply);
  bool send(intptr_t socket, const std::vector<uint8_t>& data);
  void close(Client& client);

  libretro::LoggerComponent* _logger;
  intptr_t _listener;
  unsigned _slot;
  std::string _path; // the Unix socket's
  std::vector<Client> _clients;
};
