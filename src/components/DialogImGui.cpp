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

// Dialog implementation for the macOS/Linux build: the controls are rendered as a modal ImGui window.

#include "Dialog.h"

#include "Application.h"
#include "Gui.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <cstring>

Dialog::Dialog() = default;
Dialog::~Dialog() = default;

void Dialog::init(const char* title)
{
  _title = title;
  _width = _height = 0;
  _controlData.clear();
}

static void grow(WORD& width, WORD& height, WORD x, WORD y, WORD w, WORD h)
{
  // coordinates are signed (controls may be nudged above the first row)
  const int right = (int16_t)x + (int16_t)w;
  const int bottom = (int16_t)y + (int16_t)h;
  if (right > (int16_t)width)
    width = (WORD)right;
  if (bottom > (int16_t)height)
    height = (WORD)bottom;
}

void Dialog::addCheckbox(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool* checked)
{
  ControlData cd;
  cd._type = kCheckbox;
  cd._id = id;
  cd._caption = caption;
  cd._x = x; cd._y = y; cd._w = w; cd._h = h;
  cd._checked = checked;
  _controlData.push_back(std::move(cd));
  grow(_width, _height, x, y, w, h);
}

void Dialog::addLabel(const char* caption, WORD x, WORD y, WORD w, WORD h)
{
  addLabel(caption, 0, x, y, w, h);
}

void Dialog::addLabel(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h)
{
  ControlData cd;
  cd._type = kLabel;
  cd._id = id;
  cd._caption = caption;
  cd._x = x; cd._y = y; cd._w = w; cd._h = h;
  _controlData.push_back(std::move(cd));
  grow(_width, _height, x, y, w, h);
}

void Dialog::addButton(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool isDefault)
{
  ControlData cd;
  cd._type = kButton;
  cd._id = id;
  cd._caption = caption;
  cd._x = x; cd._y = y; cd._w = w; cd._h = h;
  cd._isDefault = isDefault;
  _controlData.push_back(std::move(cd));
  grow(_width, _height, x, y, w, h);
}

void Dialog::addCombobox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD maxDropDownHeight, GetOption get_option, void* udata, int* selected)
{
  ControlData cd;
  cd._type = kCombobox;
  cd._id = id;
  cd._x = x; cd._y = y; cd._w = w; cd._h = h;
  cd._maxDropDownHeight = maxDropDownHeight;
  cd._getOption = get_option;
  cd._udata = udata;
  cd._selected = selected;
  _controlData.push_back(std::move(cd));
  grow(_width, _height, x, y, w, h);
}

void Dialog::addEditbox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, char* contents, size_t maxSize, bool readOnly)
{
  ControlData cd;
  cd._type = kEditbox;
  cd._id = id;
  cd._x = x; cd._y = y; cd._w = w; cd._h = h;
  cd._lines = lines;
  cd._contents = contents;
  cd._maxSize = maxSize;
  cd._readOnly = readOnly;
  _controlData.push_back(std::move(cd));
  grow(_width, _height, x, y, w, h);
}

Dialog::ControlData* Dialog::findControl(DWORD id)
{
  for (auto& cd : _controlData)
  {
    if (cd._id == id)
      return &cd;
  }

  return nullptr;
}

void Dialog::setControlText(DWORD id, const std::string& text)
{
  ControlData* cd = findControl(id);
  if (cd == nullptr)
    return;

  if (cd->_type == kEditbox)
    cd->_text = text;
  else
    cd->_caption = text;
}

void Dialog::close(DWORD result)
{
  _result = result;
  _closed = true;
}

void Dialog::initControls()
{
  for (auto& cd : _controlData)
  {
    switch (cd._type)
    {
      case kCheckbox:
        cd._checkedValue = *cd._checked;
        break;

      case kCombobox:
        cd._options.clear();
        if (cd._getOption == nullptr)
          break;

        for (int index = 0;; index++)
        {
          const char* option = cd._getOption(index, cd._udata);
          if (option == nullptr)
            break;

          cd._options.emplace_back(option);
        }
        cd._selectedValue = cd._selected ? *cd._selected : 0;
        break;

      case kEditbox:
        cd._text = cd._contents ? cd._contents : "";
        break;

      default:
        break;
    }
  }
}

void Dialog::retrieveData()
{
  for (auto& cd : _controlData)
  {
    switch (cd._type)
    {
      case kCheckbox:
        _updated = _updated || cd._checkedValue != *cd._checked;
        *cd._checked = cd._checkedValue;
        break;

      case kCombobox:
        if (cd._selected == nullptr)
          break;
        _updated = _updated || cd._selectedValue != *cd._selected;
        *cd._selected = cd._selectedValue;
        break;

      case kEditbox:
        if (cd._maxSize > 0)
        {
          strncpy(cd._contents, cd._text.c_str(), cd._maxSize - 1);
          cd._contents[cd._maxSize - 1] = '\0';
        }
        break;

      default:
        break;
    }
  }
}

// Dialog layouts are in Win32 dialog units: 1/4 of the average character width horizontally and 1/8 of the
// font height vertically.
// ImGui's default font is monospaced and wider than the proportional Windows dialog font, so text gets
// some extra room. Coordinates are signed, like in a Win32 dialog template.
static float unitsToPixelsX(WORD units) { return (int16_t)units * ImGui::CalcTextSize("M").x * 0.35f; }
static float unitsToPixelsY(WORD units) { return (int16_t)units * ImGui::GetFontSize() / 8.0f * 1.15f; }

void Dialog::render()
{
  const ImVec2 origin = ImGui::GetCursorPos();
  const float width = unitsToPixelsX(_width);
  const float height = unitsToPixelsY(_height);
  ImGui::Dummy(ImVec2(width, height)); // reserve the dialog area so the window sizes itself

  const bool enterPressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
  const bool escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
  DWORD clicked = 0;
  DWORD changed = 0;

  for (auto& cd : _controlData)
  {
    if (!cd._visible)
      continue;

    const ImVec2 pos(origin.x + unitsToPixelsX(cd._x), origin.y + unitsToPixelsY(cd._y));
    const float w = unitsToPixelsX(cd._w);
    const float h = unitsToPixelsY(cd._type == kEditbox && cd._lines > 1 ? cd._h * cd._lines : cd._h);
    ImGui::SetCursorPos(pos);
    ImGui::PushID((int)(&cd - _controlData.data()));

    switch (cd._type)
    {
      case kLabel:
        // only wrap labels that were laid out for several lines; the wider font would otherwise wrap
        // single-line labels that just fit on Windows
        if (h >= ImGui::GetTextLineHeightWithSpacing() * 1.5f)
        {
          ImGui::PushTextWrapPos(pos.x + w);
          ImGui::TextUnformatted(cd._caption.c_str());
          ImGui::PopTextWrapPos();
        }
        else
        {
          ImGui::TextUnformatted(cd._caption.c_str());
        }
        break;

      case kCheckbox:
        ImGui::Checkbox(cd._caption.c_str(), &cd._checkedValue);
        break;

      case kCombobox:
      {
        ImGui::SetNextItemWidth(w);
        const char* preview = (cd._selectedValue >= 0 && cd._selectedValue < (int)cd._options.size())
          ? cd._options[cd._selectedValue].c_str() : "";
        if (ImGui::BeginCombo("##combo", preview))
        {
          for (int i = 0; i < (int)cd._options.size(); ++i)
          {
            const bool selected = (i == cd._selectedValue);
            if (ImGui::Selectable(cd._options[i].c_str(), selected) && !selected)
            {
              cd._selectedValue = i;
              changed = cd._id;
            }
            if (selected)
              ImGui::SetItemDefaultFocus();
          }
          ImGui::EndCombo();
        }
        break;
      }

      case kEditbox:
      {
        const ImGuiInputTextFlags flags = cd._readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
        if (cd._lines > 1)
        {
          ImGui::InputTextMultiline("##edit", &cd._text, ImVec2(w, h), flags);
        }
        else
        {
          ImGui::SetNextItemWidth(w);
          ImGui::InputText("##edit", &cd._text, flags);
        }
        break;
      }

      case kButton:
        if (cd._isDefault)
          ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(cd._caption.c_str(), ImVec2(w, h)))
          clicked = cd._id;
        if (cd._isDefault)
          ImGui::PopStyleColor();
        break;
    }

    ImGui::PopID();
  }

  // keyboard shortcuts of a Win32 dialog: Enter presses the default button, Escape cancels
  if (clicked == 0 && !ImGui::IsAnyItemActive())
  {
    if (enterPressed)
    {
      for (const auto& cd : _controlData)
      {
        if (cd._type == kButton && cd._isDefault)
          clicked = cd._id;
      }
    }
    else if (escapePressed)
    {
      clicked = IDCANCEL;
    }
  }

  if (changed != 0)
    onSelectionChanged(changed);

  if (clicked == IDOK)
  {
    retrieveData();
    close(IDOK);
  }
  else if (clicked == IDCANCEL)
  {
    close(IDCANCEL);
  }
  else if (clicked != 0)
  {
    onButton(clicked);
  }
}

bool Dialog::show()
{
  extern Application app;
  const bool hasBackgroundInput = app.config().getBackgroundInput();

  if (hasBackgroundInput) /* disable background input while dialog is open */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

  _updated = false;
  _closed = false;
  _result = 0;
  initControls();

  gui::runModal(_title.c_str(),
    [this]()
    {
      render();
      if (!_closed)
        onFrame();
      return !_closed;
    },
    [this](const SDL_Event& event) { return onEvent(event); });

  markClosed();

  if (hasBackgroundInput)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

  return _updated;
}
