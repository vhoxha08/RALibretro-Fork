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
  bool s_initialized = false;
  std::function<void(int*, int*)> s_getSize;
  std::function<float()> s_getScale;
  // text typed into the window comes from the main thread
  std::mutex s_textInputMutex;
  std::string s_textInput;
  Uint64 s_lastFrameTime = 0;
  std::function<void()> s_toolkitRenderer;

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
}

bool gui::init(std::function<void(int*, int*)> getSize, std::function<float()> getScale)
{
  s_getSize = std::move(getSize);
  s_getScale = std::move(getScale);

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
      return io.WantCaptureKeyboard;

    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
    case SDL_MOUSEWHEEL:
      return io.WantCaptureMouse;

    default:
      return false;
  }
}

void gui::addTextInput(const char* utf8)
{
  std::lock_guard<std::mutex> lock(s_textInputMutex);
  s_textInput += utf8;
}

void gui::setToolkitRenderer(std::function<void()> renderer)
{
  s_toolkitRenderer = std::move(renderer);
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
  const float scale = s_getScale ? s_getScale() : 1.0f;
  io.DisplayFramebufferScale = ImVec2(scale, scale);

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

  // the toolkit's overlay
  if (s_toolkitRenderer)
    s_toolkitRenderer();

  ImGui::Render();

  // the game image may have changed the viewport and blending state
  glViewport(0, 0, (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x + 0.5f),
             (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y + 0.5f));
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

#endif
