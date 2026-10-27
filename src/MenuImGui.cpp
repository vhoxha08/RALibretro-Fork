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

// The main menu for the macOS/Linux build. Mirrors menu.rc; selecting an item runs the same
// Application::handleCommand as the Win32 menu. Item states are derived from the application state
// each time the menu is drawn instead of being pushed into the menu.

#ifndef _WIN32

#include "Application.h"
#include "Emulator.h"
#include "Util.h"
#include "resource.h"

#include <imgui.h>

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

// Win32 menu captions use '&' to mark the accelerator; ImGui shows text as-is.
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

void Application::menuItem(const char* label, unsigned id, bool enabled, bool checked)
{
  if (ImGui::MenuItem(label, nullptr, checked, enabled && isMenuCommandEnabled(id)))
    _pendingCommand = id;
}

void Application::drawSystemsMenu()
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
    menuItem("No Systems Found", IDM_MANAGE_CORES);
    return;
  }

  const auto drawSystem = [this](const std::string& systemName, int system)
  {
    if (!ImGui::BeginMenu(systemName.c_str()))
      return;

    std::set<std::string> cores;
    getAvailableSystemCores(system, cores);

    std::map<std::string, int> items;
    for (const auto& core : cores)
      items.emplace(getEmulatorName(core, system), encodeCoreName(core, system));

    for (const auto& item : items)
      menuItem(item.first.c_str(), IDM_SYSTEM_FIRST + item.second);

    ImGui::EndMenu();
  };

  if (systemMap.size() > 20)
  {
    // group by manufacturer, like the Win32 menu
    std::map<std::string, std::vector<std::pair<std::string, int>>> manufacturerMap;
    for (const auto& pair : systemMap)
      manufacturerMap[getSystemManufacturer(pair.second)].push_back(pair);

    for (const auto& manufacturer : manufacturerMap)
    {
      if (ImGui::BeginMenu(manufacturer.first.c_str()))
      {
        for (const auto& system : manufacturer.second)
          drawSystem(system.first, system.second);
        ImGui::EndMenu();
      }
    }
  }
  else
  {
    for (const auto& pair : systemMap)
      drawSystem(pair.first, pair.second);
  }
}

void Application::drawDiscMenu()
{
  const unsigned numDiscs = _core.getNumDiscs();
  if (!ImGui::BeginMenu(_isDriveFloppy ? "Floppy Drive" : "CD-ROM"))
    return;

  const bool trayOpen = _core.getTrayOpen();
  const char* trayLabel = _isDriveFloppy ? (trayOpen ? "Insert Disk" : "Remove Disk")
                                         : (trayOpen ? "Close Tray" : "Open Tray");
  menuItem(trayLabel, IDM_CD_OPEN_TRAY, numDiscs > 0);

  const unsigned selectedDisc = _core.getCurrentDiscIndex();
  for (unsigned i = 0; i < numDiscs && IDM_CD_DISC_FIRST + i <= IDM_CD_DISC_LAST; ++i)
  {
    const std::string label = getDiscLabel(i) + "##disc" + std::to_string(i);
    menuItem(label.c_str(), IDM_CD_DISC_FIRST + i, trayOpen, i == selectedDisc);
  }

  ImGui::EndMenu();
}

void Application::runPendingCommand()
{
  if (_pendingCommand == 0)
    return;

  const unsigned cmd = _pendingCommand;
  _pendingCommand = 0;

  // the quit request may have been handled in the same pass; the core is gone by then
  if (_fsm.currentState() == Fsm::State::Quit)
    return;

  handleCommand(cmd);
}

void Application::drawMenuBar()
{
  if (ImGui::BeginMenu("File"))
  {
    if (ImGui::BeginMenu("Select Core"))
    {
      drawSystemsMenu();
      ImGui::EndMenu();
    }

    ImGui::Separator();
    menuItem("Load Game...", IDM_LOAD_GAME);

    if (ImGui::BeginMenu("Load Recent"))
    {
      for (size_t i = 0; i < 10; ++i)
      {
        if (i < _recentList.size())
        {
          const auto& recent = _recentList[i];
          std::string caption = util::fileName(recent.path) + " (" +
            getEmulatorName(recent.coreName, recent.system) + " - " + getSystemName(recent.system) + ")";
          caption += "##recent" + std::to_string(i);
          menuItem(caption.c_str(), IDM_LOAD_RECENT_1 + i);
        }
        else
        {
          const std::string caption = "Empty##recent" + std::to_string(i);
          menuItem(caption.c_str(), IDM_LOAD_RECENT_1 + i, false);
        }
      }
      ImGui::EndMenu();
    }

    ImGui::Separator();
    menuItem("Pause Game", IDM_PAUSE_GAME);
    menuItem("Resume Game", IDM_RESUME_GAME);
    menuItem("Turbo", IDM_TURBO_GAME, true, _fastForwardChecked);
    menuItem("Reset Game", IDM_RESET_GAME);

    ImGui::Separator();
    drawDiscMenu();

    ImGui::Separator();
    if (ImGui::BeginMenu("Save Game State"))
    {
      for (unsigned i = 0; i < 10; ++i)
      {
        const std::string caption = "Slot #" + std::to_string(i + 1);
        menuItem(caption.c_str(), IDM_SAVE_STATE_1 + i, isGameActive());
      }
      ImGui::EndMenu();
    }
    menuItem("Save Game State...", IDM_SAVE_STATE, isGameActive());

    if (ImGui::BeginMenu("Load Game State"))
    {
      for (unsigned i = 0; i < 10; ++i)
      {
        const unsigned ndx = i + 1;
        const bool valid = (_validSlots & (1 << ndx)) != 0 && !hardcore();
        const std::string caption = "Slot #" + std::to_string(ndx);
        menuItem(caption.c_str(), IDM_LOAD_STATE_1 + i, valid);
      }
      ImGui::EndMenu();
    }
    menuItem("Load Game State...", IDM_LOAD_STATE);

    ImGui::Separator();
    menuItem("Exit", IDM_EXIT);
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Settings"))
  {
    menuItem("Core Settings...", IDM_CORE_CONFIG);
    if (ImGui::BeginMenu("Input"))
    {
      menuItem("Hot Keys...", IDM_INPUT_CONFIG);
      menuItem("Controller 1...", IDM_INPUT_CONTROLLER_1);
      menuItem("Controller 2...", IDM_INPUT_CONTROLLER_2);
      menuItem("Controller 3...", IDM_INPUT_CONTROLLER_3);
      menuItem("Controller 4...", IDM_INPUT_CONTROLLER_4);
      menuItem("Background Input", IDM_INPUT_BACKGROUND_INPUT, true, _config.getBackgroundInput());
      ImGui::EndMenu();
    }
    menuItem("Emulator...", IDM_EMULATOR_CONFIG);
    menuItem("Saving...", IDM_SAVING_CONFIG);
    menuItem("Video...", IDM_VIDEO_CONFIG);
    if (ImGui::BeginMenu("Window Size"))
    {
      for (unsigned i = 0; i < 5; ++i)
      {
        const std::string caption = "Resize to " + std::to_string(i + 1) + "x";
        menuItem(caption.c_str(), IDM_WINDOW_1X + i);
      }
      ImGui::EndMenu();
    }
    ImGui::Separator();
    menuItem("Manage Cores...", IDM_MANAGE_CORES);
    ImGui::EndMenu();
  }

  // RetroAchievements menu, built by the toolkit (RebuildMenu adds it on Windows)
  RA_MenuItem raItems[64];
  const int numRaItems = RA_GetPopupMenuItems(raItems);
  if (numRaItems > 0 && ImGui::BeginMenu("RetroAchievements"))
  {
    for (int i = 0; i < numRaItems; ++i)
    {
      if (raItems[i].nID == 0)
      {
        ImGui::Separator();
        continue;
      }

      const std::string caption = stripAccelerator(narrow(raItems[i].sLabel)) + "##ra" + std::to_string(raItems[i].nID);
      menuItem(caption.c_str(), static_cast<unsigned>(raItems[i].nID), true, raItems[i].bChecked != 0);
    }
    ImGui::EndMenu();
  }

  if (ImGui::MenuItem("About", nullptr, false, isMenuCommandEnabled(IDM_ABOUT)))
    _pendingCommand = IDM_ABOUT;
}

#endif
