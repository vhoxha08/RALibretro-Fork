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

// The main menu for the macOS/Linux build (a native menu bar, see host/Host.h). Mirrors menu.rc; selecting an
// item runs the same Application::handleCommand as the Win32 menu. The menus are rebuilt from the application
// state each time they open instead of having their items updated. They are shown by the main thread, but built
// and handled on the game thread, which owns the application state.

#ifndef _WIN32

#include "Application.h"
#include "Emulator.h"
#include "Util.h"
#include "resource.h"

#include <map>
#include <set>

static std::string narrow(const wchar_t* text)
{
  std::string result;
  if (text == nullptr)
    return result;

  for (; *text; ++text)
  {
    const uint32_t c = static_cast<uint32_t>(*text);
    if (c < 0x80)
    {
      result.push_back(static_cast<char>(c));
    }
    else if (c < 0x800)
    {
      result.push_back(static_cast<char>(0xC0 | (c >> 6)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    else if (c < 0x10000)
    {
      result.push_back(static_cast<char>(0xE0 | (c >> 12)));
      result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
    else
    {
      result.push_back(static_cast<char>(0xF0 | (c >> 18)));
      result.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
    }
  }

  return result;
}

// Win32 menu captions use '&' to mark the accelerator; native menus here show text as-is.
static std::string stripAccelerator(const std::string& caption)
{
  std::string result;
  for (size_t i = 0; i < caption.size(); ++i)
  {
    if (caption[i] == '&')
    {
      if (i + 1 < caption.size() && caption[i + 1] == '&')
        result.push_back(caption[++i]);
      continue;
    }

    result.push_back(caption[i]);
  }
  return result;
}

void Application::menuItem(host::MenuSink& sink, const std::string& label, unsigned id, bool enabled, bool checked)
{
  sink.item(label, id, enabled && isMenuCommandEnabled(id), checked);
}

namespace
{
  // Records a menu (on the game thread) for the main thread to show.
  template <typename Entry>
  class RecordingSink : public host::MenuSink
  {
  public:
    explicit RecordingSink(std::vector<Entry>& entries) : _entries(entries) {}

    void beginMenu(const std::string& label) override { _entries.push_back({Entry::Kind::Begin, label, 0, true, false}); }
    void endMenu() override { _entries.push_back({Entry::Kind::End, std::string(), 0, true, false}); }
    void item(const std::string& label, unsigned id, bool enabled, bool checked) override
    {
      _entries.push_back({Entry::Kind::Item, label, id, enabled, checked});
    }
    void separator() override { _entries.push_back({Entry::Kind::Separator, std::string(), 0, true, false}); }

  private:
    std::vector<Entry>& _entries;
  };
}

void Application::buildMenu(host::Menu menu, host::MenuSink& sink)
{
  // main thread: the game thread knows the state. It answers within a frame, unless it is busy (i.e. loading a
  // game) or waiting for the main thread itself (a message box); then show the menu as it was last time.
  std::vector<MenuEntry> entries;
  if (queryGameThread([this, menu, &entries]() {
        RecordingSink<MenuEntry> recorder(entries);
        buildMenuItems(menu, recorder);
      }, std::chrono::milliseconds(250)))
  {
    _menuCache[menu] = entries;
  }
  else
  {
    entries = _menuCache[menu];
  }

  for (const auto& entry : entries)
  {
    switch (entry.kind)
    {
      case MenuEntry::Kind::Begin: sink.beginMenu(entry.label); break;
      case MenuEntry::Kind::End: sink.endMenu(); break;
      case MenuEntry::Kind::Item: sink.item(entry.label, entry.id, entry.enabled, entry.checked); break;
      case MenuEntry::Kind::Separator: sink.separator(); break;
    }
  }
}

void Application::onMenuCommand(unsigned id)
{
  // handled by the game loop (like the Win32 WM_COMMAND)
  postToGameThread([this, id]() {
    // the quit request may have been handled first; the core is gone by then
    if (_fsm.currentState() != Fsm::State::Quit)
      handleCommand(id);
  });
}

void Application::addSystemsMenu(host::MenuSink& sink)
{
  std::set<int> availableSystems;
  getAvailableSystems(availableSystems);

  // use map to sort labels
  std::set<std::string> systemCores;
  std::map<std::string, int> systemMap;
  for (auto system : availableSystems)
  {
    systemCores.clear();
    getAvailableSystemCores(system, systemCores);
    if (!systemCores.empty())
      systemMap.emplace(getSystemName(system), system);
  }

  if (systemMap.empty())
  {
    menuItem(sink, "No Systems Found", IDM_MANAGE_CORES);
    return;
  }

  const auto addSystem = [this, &sink](const std::string& systemName, int system)
  {
    sink.beginMenu(systemName);

    std::set<std::string> cores;
    getAvailableSystemCores(system, cores);

    std::map<std::string, int> items;
    for (const auto& core : cores)
      items.emplace(getEmulatorName(core, system), encodeCoreName(core, system));

    for (const auto& item : items)
      menuItem(sink, item.first, IDM_SYSTEM_FIRST + item.second);

    sink.endMenu();
  };

  if (systemMap.size() > 20)
  {
    // group by manufacturer, like the Win32 menu
    std::map<std::string, std::vector<std::pair<std::string, int>>> manufacturerMap;
    for (const auto& pair : systemMap)
      manufacturerMap[getSystemManufacturer(pair.second)].push_back(pair);

    for (const auto& manufacturer : manufacturerMap)
    {
      sink.beginMenu(manufacturer.first);
      for (const auto& system : manufacturer.second)
        addSystem(system.first, system.second);
      sink.endMenu();
    }
  }
  else
  {
    for (const auto& pair : systemMap)
      addSystem(pair.first, pair.second);
  }
}

void Application::addDiscMenu(host::MenuSink& sink)
{
  const unsigned numDiscs = _core.getNumDiscs();
  sink.beginMenu(_isDriveFloppy ? "Floppy Drive" : "CD-ROM");

  const bool trayOpen = _core.getTrayOpen();
  const char* trayLabel = _isDriveFloppy ? (trayOpen ? "Insert Disk" : "Remove Disk")
                                         : (trayOpen ? "Close Tray" : "Open Tray");
  menuItem(sink, trayLabel, IDM_CD_OPEN_TRAY, numDiscs > 0);

  const unsigned selectedDisc = _core.getCurrentDiscIndex();
  for (unsigned i = 0; i < numDiscs && IDM_CD_DISC_FIRST + i <= IDM_CD_DISC_LAST; ++i)
  {
    menuItem(sink, getDiscLabel(i), IDM_CD_DISC_FIRST + i, trayOpen, i == selectedDisc);
  }

  sink.endMenu();
}

void Application::runPendingCommand()
{
  runGameQueries();

  // one at a time: a command may quit
  for (;;)
  {
    std::function<void()> task;
    {
      std::lock_guard<std::mutex> lock(_gameThreadMutex);
      if (_gameTasks.empty())
        return;

      task = std::move(_gameTasks.front());
      _gameTasks.pop_front();
    }

    task();
  }
}

void Application::buildMenuItems(host::Menu menu, host::MenuSink& sink)
{
  switch (menu)
  {
    case host::Menu::File:
    {
      sink.beginMenu("Select Core");
      addSystemsMenu(sink);
      sink.endMenu();

      sink.separator();
      menuItem(sink, "Load Game...", IDM_LOAD_GAME);

      sink.beginMenu("Load Recent");
      for (size_t i = 0; i < 10; ++i)
      {
        if (i < _recentList.size())
        {
          const auto& recent = _recentList[i];
          const std::string caption = util::fileName(recent.path) + " (" +
            getEmulatorName(recent.coreName, recent.system) + " - " + getSystemName(recent.system) + ")";
          menuItem(sink, caption, IDM_LOAD_RECENT_1 + i);
        }
        else
        {
          menuItem(sink, "Empty", IDM_LOAD_RECENT_1 + i, false);
        }
      }
      sink.endMenu();

      sink.separator();
      menuItem(sink, "Pause Game", IDM_PAUSE_GAME);
      menuItem(sink, "Resume Game", IDM_RESUME_GAME);
      menuItem(sink, "Turbo", IDM_TURBO_GAME, true, _fastForwardChecked);
      menuItem(sink, "Reset Game", IDM_RESET_GAME);

      sink.separator();
      addDiscMenu(sink);

      sink.separator();
      sink.beginMenu("Save Game State");
      for (unsigned i = 0; i < 10; ++i)
        menuItem(sink, "Slot #" + std::to_string(i + 1), IDM_SAVE_STATE_1 + i, isGameActive());
      sink.endMenu();
      menuItem(sink, "Save Game State...", IDM_SAVE_STATE, isGameActive());

      sink.beginMenu("Load Game State");
      for (unsigned i = 0; i < 10; ++i)
      {
        const unsigned ndx = i + 1;
        const bool valid = (_validSlots & (1 << ndx)) != 0 && !hardcore();
        menuItem(sink, "Slot #" + std::to_string(ndx), IDM_LOAD_STATE_1 + i, valid);
      }
      sink.endMenu();
      menuItem(sink, "Load Game State...", IDM_LOAD_STATE);

      sink.separator();
      menuItem(sink, "Exit", IDM_EXIT); // macOS shows it as Quit in the application menu
      break;
    }

    case host::Menu::Settings:
      menuItem(sink, "Core Settings...", IDM_CORE_CONFIG);
      sink.beginMenu("Input");
      menuItem(sink, "Hot Keys...", IDM_INPUT_CONFIG);
      menuItem(sink, "Controller 1...", IDM_INPUT_CONTROLLER_1);
      menuItem(sink, "Controller 2...", IDM_INPUT_CONTROLLER_2);
      menuItem(sink, "Controller 3...", IDM_INPUT_CONTROLLER_3);
      menuItem(sink, "Controller 4...", IDM_INPUT_CONTROLLER_4);
      menuItem(sink, "Background Input", IDM_INPUT_BACKGROUND_INPUT, true, _config.getBackgroundInput());
      sink.endMenu();
      menuItem(sink, "Emulator...", IDM_EMULATOR_CONFIG);
      menuItem(sink, "Saving...", IDM_SAVING_CONFIG);
      menuItem(sink, "Video...", IDM_VIDEO_CONFIG);
      sink.beginMenu("Window Size");
      for (unsigned i = 0; i < 5; ++i)
        menuItem(sink, "Resize to " + std::to_string(i + 1) + "x", IDM_WINDOW_1X + i);
      sink.endMenu();
      sink.separator();
      menuItem(sink, "Manage Cores...", IDM_MANAGE_CORES);
      break;

    case host::Menu::RetroAchievements:
    {
      // built by the toolkit (RebuildMenu adds it on Windows)
      RA_MenuItem raItems[64];
      const int numRaItems = RA_GetPopupMenuItems(raItems);
      for (int i = 0; i < numRaItems; ++i)
      {
        if (raItems[i].nID == 0)
          sink.separator();
        else
          menuItem(sink, stripAccelerator(narrow(raItems[i].sLabel)), static_cast<unsigned>(raItems[i].nID), true,
                   raItems[i].bChecked != 0);
      }
      if (numRaItems == 0)
        sink.item("Not available", 0, false);
      break;
    }

    case host::Menu::Help:
      menuItem(sink, "About RALibretro", IDM_ABOUT); // macOS shows it in the application menu
      break;
  }
}

#endif
