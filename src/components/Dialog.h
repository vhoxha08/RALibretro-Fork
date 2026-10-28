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

#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include "Platform.h"
#include <string>
#endif
#include <vector>

#ifndef _WIN32

// Same API as the Win32 Dialog below. The controls are recorded and shown as a native modal dialog
// (DialogWx.cpp). Subclasses customize behavior through the hooks below instead of a Win32 dialogProc; they run
// on the main thread while the game thread waits for the dialog.
class Dialog
{
  friend class DialogWindow;

public:
  typedef const char* (*GetOption)(int index, void* udata);

  Dialog();
  virtual ~Dialog();

  void init(const char* title);
  void addCheckbox(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool* checked);
  void addLabel(const char* caption, WORD x, WORD y, WORD w, WORD h);
  void addLabel(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h);
  void addButton(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool isDefault);
  void addCombobox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD maxDropDownHeight, GetOption get_option, void* udata, int* selected);
  void addEditbox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, char* contents, size_t maxSize, bool readOnly);

  // Runs the dialog modally (from any thread). Returns true if OK was pressed and a checkbox or combobox changed.
  bool show();

protected:
  enum Type
  {
    kCheckbox,
    kCombobox,
    kEditbox,
    kLabel,
    kButton
  };

  struct ControlData
  {
    Type _type;
    DWORD _id;
    std::string _caption;
    WORD _x, _y, _w, _h;
    bool _isDefault = false;
    bool _visible = true;
    bool _enabled = true;

    bool* _checked = nullptr;
    GetOption _getOption = nullptr;
    void* _udata = nullptr;
    int* _selected = nullptr;
    WORD _maxDropDownHeight = 0;
    char* _contents = nullptr;
    size_t _maxSize = 0;
    WORD _lines = 1;
    bool _readOnly = false;

    // values while the dialog is open (copied back by retrieveData)
    bool _checkedValue = false;
    int _selectedValue = 0;
    std::vector<std::string> _options;
    std::string _text;
  };

  ControlData* findControl(DWORD id);
  void setControlText(DWORD id, const std::string& text);
  void close(DWORD result);

  // copies the bound values into the controls when the dialog opens
  virtual void initControls();
  // copies the control values back to the bound variables when OK is pressed
  virtual void retrieveData();
  // called for buttons other than OK/Cancel; return true if handled
  virtual bool onButton(DWORD id) { (void)id; return false; }
  // called when the user picks a different item in a combobox
  virtual void onSelectionChanged(DWORD id) { (void)id; }
  // called for each key pressed or released while the dialog is open (before the controls get it); return true
  // to consume it
  virtual bool onEvent(const union SDL_Event& event) { (void)event; return false; }
  // called about 60 times per second while the dialog is open
  virtual void onFrame() {}
  virtual void markClosed() {}

  std::string _title;
  WORD _width = 0;
  WORD _height = 0;
  std::vector<ControlData> _controlData;
  bool _updated = false;
  bool _closed = false;
  DWORD _result = 0;
};

#else

class Dialog
{
public:
  typedef const char* (*GetOption)(int index, void* udata);

  Dialog();
  ~Dialog();

  void init(const char* title);
  void addCheckbox(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool* checked);
  void addLabel(const char* caption, WORD x, WORD y, WORD w, WORD h);
  void addLabel(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h);
  void addButton(const char* caption, DWORD id, WORD x, WORD y, WORD w, WORD h, bool isDefault);
  void addCombobox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD maxDropDownHeight, GetOption get_option, void* udata, int* selected);
  void addEditbox(DWORD id, WORD x, WORD y, WORD w, WORD h, WORD lines, char* contents, size_t maxSize, bool readOnly);

  bool show();

protected:
  enum Type
  {
    kCheckbox,
    kCombobox,
    kEditbox
  };

  struct ControlData
  {
    Type _type;
    DWORD _id;

    union
    {
      bool* _checked;

      struct
      {
        GetOption _getOption;
        void* _udata;
        int* _selected;
      };

      struct
      {
        char* _contents;
        size_t _maxSize;
      };
    };
  };

  void align(size_t alignment);

  void writeDlgItemTemplateEx(DWORD helpId, DWORD exStyle, DWORD style, WORD x, WORD y, WORD cx, WORD cy, DWORD id, DWORD windowClass, const char* title, WORD extraCount);

  void write(void* data, size_t size);
  void writeStr(const char* str);
  void writeWide(const WCHAR* str);

  template<typename T> void write(T t)
  {
    write(&t, sizeof(t));
  }

  void update(WORD x, WORD y, WORD w, WORD h);

  virtual void initControls(HWND hwnd);
  virtual void retrieveData(HWND hwnd);

  static INT_PTR CALLBACK s_dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
  virtual INT_PTR dialogProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) { return FALSE; }
  virtual void markClosed(HWND hwnd);

  void*  _template;
  size_t _size;
  size_t _reserved;

  WORD* _numControls;
  WORD* _width;
  WORD* _height;

  std::vector<ControlData> _controlData;
  bool _updated;
};

#endif // _WIN32
