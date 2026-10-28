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

#include <cfloat>
#include <cstdio>
#include <mutex>
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
  std::function<void(int*, int*)> s_getSize;
  // text typed into the window comes from the main thread
  std::mutex s_textInputMutex;
  std::string s_textInput;
  Uint64 s_lastFrameTime = 0;
  std::function<void()> s_redraw;
  std::vector<Modal*> s_modals;
  int s_externalModals = 0; // modal windows drawn by someone else (the toolkit), see runModalLoop
  std::function<void()> s_toolkitRenderer;
  std::function<void()> s_perFrame;

  bool anyModalActive()
  {
    return !s_modals.empty() || s_externalModals > 0;
  }

  ImGuiKey toImGuiKey(SDL_Keycode key)
  {
    if (key >= SDLK_a && key <= SDLK_z)
      return (ImGuiKey)(ImGuiKey_A + (key - SDLK_a));
    if (key >= SDLK_0 && key <= SDLK_9)
      return (ImGuiKey)(ImGuiKey_0 + (key - SDLK_0));
    if (key >= SDLK_F1 && key <= SDLK_F12)
      return (ImGuiKey)(ImGuiKey_F1 + (key - SDLK_F1));
    if (key >= SDLK_KP_1 && key <= SDLK_KP_9)
      return (ImGuiKey)(ImGuiKey_Keypad1 + (key - SDLK_KP_1));

    switch (key)
    {
      case SDLK_TAB:          return ImGuiKey_Tab;
      case SDLK_LEFT:         return ImGuiKey_LeftArrow;
      case SDLK_RIGHT:        return ImGuiKey_RightArrow;
      case SDLK_UP:           return ImGuiKey_UpArrow;
      case SDLK_DOWN:         return ImGuiKey_DownArrow;
      case SDLK_PAGEUP:       return ImGuiKey_PageUp;
      case SDLK_PAGEDOWN:     return ImGuiKey_PageDown;
      case SDLK_HOME:         return ImGuiKey_Home;
      case SDLK_END:          return ImGuiKey_End;
      case SDLK_INSERT:       return ImGuiKey_Insert;
      case SDLK_DELETE:       return ImGuiKey_Delete;
      case SDLK_BACKSPACE:    return ImGuiKey_Backspace;
      case SDLK_SPACE:        return ImGuiKey_Space;
      case SDLK_RETURN:       return ImGuiKey_Enter;
      case SDLK_ESCAPE:       return ImGuiKey_Escape;
      case SDLK_QUOTE:        return ImGuiKey_Apostrophe;
      case SDLK_COMMA:        return ImGuiKey_Comma;
      case SDLK_MINUS:        return ImGuiKey_Minus;
      case SDLK_PERIOD:       return ImGuiKey_Period;
      case SDLK_SLASH:        return ImGuiKey_Slash;
      case SDLK_SEMICOLON:    return ImGuiKey_Semicolon;
      case SDLK_EQUALS:       return ImGuiKey_Equal;
      case SDLK_LEFTBRACKET:  return ImGuiKey_LeftBracket;
      case SDLK_BACKSLASH:    return ImGuiKey_Backslash;
      case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
      case SDLK_BACKQUOTE:    return ImGuiKey_GraveAccent;
      case SDLK_KP_0:         return ImGuiKey_Keypad0;
      case SDLK_KP_ENTER:     return ImGuiKey_KeypadEnter;
      case SDLK_LCTRL:        return ImGuiKey_LeftCtrl;
      case SDLK_LSHIFT:       return ImGuiKey_LeftShift;
      case SDLK_LALT:         return ImGuiKey_LeftAlt;
      case SDLK_LGUI:         return ImGuiKey_LeftSuper;
      case SDLK_RCTRL:        return ImGuiKey_RightCtrl;
      case SDLK_RSHIFT:       return ImGuiKey_RightShift;
      case SDLK_RALT:         return ImGuiKey_RightAlt;
      case SDLK_RGUI:         return ImGuiKey_RightSuper;
      default:                return ImGuiKey_None;
    }
  }

  // Feeds an SDL event (from SDL or synthesized by the native window) to ImGui.
  void addEvent(ImGuiIO& io, const SDL_Event& event)
  {
    switch (event.type)
    {
      case SDL_MOUSEMOTION:
        io.AddMousePosEvent((float)event.motion.x, (float)event.motion.y);
        break;

      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP:
      {
        int button = -1;
        switch (event.button.button)
        {
          case SDL_BUTTON_LEFT:   button = 0; break;
          case SDL_BUTTON_RIGHT:  button = 1; break;
          case SDL_BUTTON_MIDDLE: button = 2; break;
        }
        if (button >= 0)
        {
          io.AddMousePosEvent((float)event.button.x, (float)event.button.y);
          io.AddMouseButtonEvent(button, event.type == SDL_MOUSEBUTTONDOWN);
        }
        break;
      }

      case SDL_MOUSEWHEEL:
        io.AddMouseWheelEvent(-event.wheel.preciseX, event.wheel.preciseY);
        break;

      case SDL_TEXTINPUT:
        io.AddInputCharactersUTF8(event.text.text);
        break;

      case SDL_KEYDOWN:
      case SDL_KEYUP:
      {
        const Uint16 mod = event.key.keysym.mod;
        io.AddKeyEvent(ImGuiMod_Ctrl, (mod & KMOD_CTRL) != 0);
        io.AddKeyEvent(ImGuiMod_Shift, (mod & KMOD_SHIFT) != 0);
        io.AddKeyEvent(ImGuiMod_Alt, (mod & KMOD_ALT) != 0);
        io.AddKeyEvent(ImGuiMod_Super, (mod & KMOD_GUI) != 0);

        const ImGuiKey key = toImGuiKey(event.key.keysym.sym);
        if (key != ImGuiKey_None)
          io.AddKeyEvent(key, event.type == SDL_KEYDOWN);
        break;
      }

      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
          io.AddFocusEvent(true);
        else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
          io.AddFocusEvent(false);
        else if (event.window.event == SDL_WINDOWEVENT_LEAVE)
          io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    }
  }

  // Pumps events and redraws until isOpen returns false or the application is asked to quit.
  // Returns false if quitting (the quit request is put back for the main loop).
  bool modalLoop(const std::function<bool()>& isOpen, const std::function<bool(const SDL_Event&)>& onEvent,
                 const std::function<bool()>& afterFrame)
  {
    while (isOpen())
    {
      // the main thread pumps SDL's events
      SDL_Event event;
      while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT) > 0)
      {
        if (event.type == SDL_QUIT)
        {
          SDL_PushEvent(&event);
          return false;
        }

        if (onEvent && onEvent(event))
          continue;

        gui::processEvent(event);
      }

      if (s_redraw)
        s_redraw();

      // work queued during the frame (i.e. by the toolkit's windows) runs outside of it
      if (s_perFrame)
        s_perFrame();

      if (afterFrame && isOpen() && !afterFrame())
        break;

      SDL_Delay(16);
    }

    return true;
  }
}

bool gui::init(std::function<void(int*, int*)> getSize)
{
  s_getSize = std::move(getSize);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr; // window positions are not persisted
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  // ImGui's built-in font only covers ASCII. Merge system fonts as fallbacks for the symbols the toolkit uses
  // (i.e. the controller button glyphs in the overlay); ImGui 1.92 loads their glyphs on demand.
  io.Fonts->AddFontDefault();
  static const char* const fallbackFonts[] = {
    "/System/Library/Fonts/Apple Symbols.ttf",
    "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/noto/NotoSansSymbols2-Regular.ttf",
  };
  for (const char* path : fallbackFonts)
  {
    if (FILE* file = fopen(path, "rb"))
    {
      fclose(file);
      ImFontConfig config;
      config.MergeMode = true;
      io.Fonts->AddFontFromFileTTF(path, 0.0f, &config); // size follows the font it is merged into
    }
  }

  io.BackendPlatformName = "ralibretro";
  if (!ImGui_ImplOpenGL3_Init("#version 150"))
    return false;

  s_initialized = true;
  return true;
}

void gui::destroy()
{
  if (!s_initialized)
    return;

  ImGui_ImplOpenGL3_Shutdown();
  ImGui::DestroyContext();
  s_initialized = false;
}

void gui::setRedraw(std::function<void()> redraw)
{
  s_redraw = std::move(redraw);
}

bool gui::processEvent(const SDL_Event& event)
{
  if (!s_initialized)
    return false;

  ImGuiIO& io = ImGui::GetIO();
  addEvent(io, event);

  switch (event.type)
  {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
    case SDL_TEXTINPUT:
      return io.WantCaptureKeyboard || anyModalActive();

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
      return io.WantCaptureMouse || anyModalActive();

    default:
      return false;
  }
}

void gui::addTextInput(const char* utf8)
{
  std::lock_guard<std::mutex> lock(s_textInputMutex);
  s_textInput += utf8;
}

bool gui::isModalActive()
{
  return anyModalActive();
}

void gui::setToolkitRenderer(std::function<void()> renderer)
{
  s_toolkitRenderer = std::move(renderer);
}

void gui::setPerFrame(std::function<void()> perFrame)
{
  s_perFrame = std::move(perFrame);
}

bool gui::runModalLoop(const std::function<bool()>& isOpen)
{
  if (!s_initialized)
    return true;

  ++s_externalModals;
  const bool result = modalLoop(isOpen, nullptr, nullptr);
  --s_externalModals;

  if (s_redraw)
    s_redraw();

  return result;
}

void gui::render()
{
  if (!s_initialized)
    return;

  ImGuiIO& io = ImGui::GetIO();
  int width = 0, height = 0;
  if (s_getSize)
    s_getSize(&width, &height);
  io.DisplaySize = ImVec2((float)width, (float)height);
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f); // the game view draws at 1x (host/HostMac.mm)

  const Uint64 now = SDL_GetPerformanceCounter();
  io.DeltaTime = s_lastFrameTime ? (float)((double)(now - s_lastFrameTime) / (double)SDL_GetPerformanceFrequency()) :
                                   1.0f / 60.0f;
  if (io.DeltaTime <= 0.0f)
    io.DeltaTime = 1.0f / 1000.0f;
  s_lastFrameTime = now;

  {
    std::lock_guard<std::mutex> lock(s_textInputMutex);
    if (!s_textInput.empty())
    {
      io.AddInputCharactersUTF8(s_textInput.c_str());
      s_textInput.clear();
    }
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();

  // the toolkit's windows (login, message boxes, ...)
  if (s_toolkitRenderer)
    s_toolkitRenderer();

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

  const bool notQuitting = modalLoop([&modal]() { return modal.open; }, onEvent, afterFrame);

  s_modals.pop_back();

  // draw once more so the closed dialog disappears immediately
  if (s_redraw)
    s_redraw();

  return notQuitting;
}

#endif
