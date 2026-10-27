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

#include "Gui.h"

#ifndef _WIN32

#include <imgui.h>
#include <SDL_opengl.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#include <string>
#include <vector>

namespace
{
  struct Modal
  {
    std::string title;
    const std::function<bool()>* draw;
    bool open;
  };

  bool s_initialized = false;
  bool s_menuBarVisible = true;
  std::function<void()> s_menuRenderer;
  std::function<void()> s_redraw;
  std::vector<Modal*> s_modals;
}

bool gui::init(SDL_Window* window, SDL_GLContext glContext)
{
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr; // window positions are not persisted
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  if (!ImGui_ImplSDL2_InitForOpenGL(window, glContext))
    return false;

  if (!ImGui_ImplOpenGL3_Init("#version 150"))
  {
    ImGui_ImplSDL2_Shutdown();
    return false;
  }

  s_initialized = true;
  return true;
}

void gui::destroy()
{
  if (!s_initialized)
    return;

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  s_initialized = false;
}

void gui::setMenuRenderer(std::function<void()> renderer)
{
  s_menuRenderer = std::move(renderer);
}

void gui::setRedraw(std::function<void()> redraw)
{
  s_redraw = std::move(redraw);
}

bool gui::processEvent(const SDL_Event& event)
{
  if (!s_initialized)
    return false;

  ImGui_ImplSDL2_ProcessEvent(&event);

  const ImGuiIO& io = ImGui::GetIO();
  switch (event.type)
  {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
      return io.WantCaptureKeyboard || !s_modals.empty();

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
      return io.WantCaptureMouse || !s_modals.empty();

    default:
      return false;
  }
}

int gui::menuBarHeight()
{
  if (!s_initialized || !s_menuBarVisible)
    return 0;

  // same as ImGui::GetFrameHeight(), which is only valid inside a frame
  const ImGuiStyle& style = ImGui::GetStyle();
  return (int)(style.FontSizeBase + style.FramePadding.y * 2.0f + 0.5f);
}

void gui::setMenuBarVisible(bool visible)
{
  s_menuBarVisible = visible;
}

bool gui::isModalActive()
{
  return !s_modals.empty();
}

void gui::render()
{
  if (!s_initialized)
    return;

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  if (s_menuBarVisible && s_menuRenderer)
  {
    ImGui::BeginDisabled(!s_modals.empty());
    if (ImGui::BeginMainMenuBar())
    {
      s_menuRenderer();
      ImGui::EndMainMenuBar();
    }
    ImGui::EndDisabled();
  }

  // only the innermost modal is interactive; it is drawn last so it is on top
  for (size_t i = 0; i < s_modals.size(); ++i)
  {
    Modal* modal = s_modals[i];
    const bool interactive = (i + 1 == s_modals.size());

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetWorkCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    // on Windows dialogs are separate windows; here they must fit in the main window, so large ones scroll
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(viewport->WorkSize.x, viewport->WorkSize.y));
    ImGui::BeginDisabled(!interactive);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_HorizontalScrollbar;
    bool open = true;
    if (ImGui::Begin(modal->title.c_str(), interactive ? &open : nullptr, flags))
    {
      if (interactive && ImGui::IsWindowAppearing())
        ImGui::SetWindowFocus();

      if (!(*modal->draw)())
        modal->open = false;
    }
    ImGui::End();
    ImGui::EndDisabled();

    if (!open)
      modal->open = false;
  }

  ImGui::Render();

  // the game image may have changed the viewport and blending state
  ImGuiIO& io = ImGui::GetIO();
  glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool gui::runModal(const char* title, const std::function<bool()>& draw,
                   const std::function<bool(const SDL_Event&)>& onEvent,
                   const std::function<bool()>& afterFrame)
{
  if (!s_initialized)
    return true;

  Modal modal{title, &draw, true};
  s_modals.push_back(&modal);

  bool quitting = false;
  while (modal.open)
  {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_QUIT)
      {
        // close the dialog and let the main loop see the quit request
        quitting = true;
        modal.open = false;
        SDL_PushEvent(&event);
        break;
      }

      if (onEvent && onEvent(event))
        continue;

      processEvent(event);
    }

    if (s_redraw)
      s_redraw();

    if (modal.open && afterFrame && !afterFrame())
      modal.open = false;

    SDL_Delay(16);
  }

  s_modals.pop_back();

  // draw once more so the closed dialog disappears immediately
  if (s_redraw)
    s_redraw();

  return !quitting;
}

#endif
