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
  if (x + w > width)
    width = x + w;
  if (y + h > height)
    height = y + h;
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
        for (int index = 0;; index++)
        {
          const char* option = cd._getOption(index, cd._udata);
          if (option == nullptr)
            break;

          cd._options.emplace_back(option);
        }
        cd._selectedValue = *cd._selected;
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

void Dialog::render()
{
  // implemented with ImGui (task: port settings dialogs)
}

bool Dialog::show()
{
  // TODO: run the dialog as a modal ImGui window
  _updated = false;
  _closed = false;
  initControls();
  markClosed();
  return _updated;
}
