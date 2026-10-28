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

// Dialog implementation for the macOS/Linux build: the controls are shown in a native modal dialog (wxWidgets),
// laid out like the Win32 dialog template.

#include "Dialog.h"

#include "Application.h"
#include "host/Host.h"
#include "host/HostWxInternal.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/timer.h>

#include <algorithm>
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

// The native dialog for a Dialog: built from its controls, with the hooks called as the user interacts with it.
// Hooks change the controls' data (text, visibility, options); the widgets are updated to match afterwards.
class DialogWindow : public wxDialog
{
public:
  DialogWindow(Dialog& dialog, wxWindow* parent) :
    wxDialog(parent, wxID_ANY, wxString::FromUTF8(dialog._title), wxDefaultPosition, wxDefaultSize,
             wxDEFAULT_DIALOG_STYLE),
    _dialog(dialog)
  {
    for (auto& cd : _dialog._controlData)
      _widgets.push_back(createWidget(cd));

    layout();
    sync();

    // like a Win32 dialog: Enter presses the default button (done by wx), Escape cancels. The dialog sees keys
    // before the controls (i.e. to capture a key binding).
    Bind(wxEVT_CHAR_HOOK, &DialogWindow::onCharHook, this);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { click(IDCANCEL); });

    _timer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) {
      _dialog.onFrame();
      afterHook();
    });
    _timer.Start(16);
  }

private:
  wxWindow* createWidget(Dialog::ControlData& cd)
  {
    switch (cd._type)
    {
      case Dialog::kLabel:
        return new wxStaticText(this, wxID_ANY, wxString::FromUTF8(cd._caption));

      case Dialog::kCheckbox:
      {
        auto* checkbox = new wxCheckBox(this, wxID_ANY, wxString::FromUTF8(cd._caption));
        checkbox->Bind(wxEVT_CHECKBOX, [&cd, checkbox](wxCommandEvent&) { cd._checkedValue = checkbox->GetValue(); });
        return checkbox;
      }

      case Dialog::kCombobox:
      {
        auto* choice = new wxChoice(this, wxID_ANY);
        choice->Bind(wxEVT_CHOICE, [this, &cd, choice](wxCommandEvent&) {
          if (choice->GetSelection() == cd._selectedValue)
            return;

          cd._selectedValue = choice->GetSelection();
          _dialog.onSelectionChanged(cd._id);
          afterHook();
        });
        return choice;
      }

      case Dialog::kEditbox:
      {
        long style = 0;
        if (cd._lines > 1)
          style |= wxTE_MULTILINE;
        if (cd._readOnly)
          style |= wxTE_READONLY;
        auto* text = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, style);
        if (cd._maxSize > 1 && cd._lines <= 1)
          text->SetMaxLength(cd._maxSize - 1);
        text->Bind(wxEVT_TEXT, [this, &cd, text](wxCommandEvent&) {
          if (!_syncing)
            cd._text = std::string(text->GetValue().utf8_str());
        });
        return text;
      }

      case Dialog::kButton:
      {
        auto* button = new wxButton(this, wxID_ANY, wxString::FromUTF8(cd._caption));
        if (cd._isDefault)
          button->SetDefault();
        const DWORD id = cd._id;
        button->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent&) { click(id); });
        return button;
      }
    }

    return nullptr;
  }

  // Win32 dialog units: 1/4 of the average character width horizontally and 1/8 of the character height
  // vertically. Coordinates are signed, like in a Win32 dialog template.
  wxPoint toPixels(WORD x, WORD y) { return ConvertDialogToPixels(wxPoint((int16_t)x, (int16_t)y)); }

  void layout()
  {
    // the templates leave no margin at the top and left (Windows adds some at the right and bottom only); give the
    // controls the usual 7 units all around
    int left = 0x7FFF, top = 0x7FFF;
    for (const auto& cd : _dialog._controlData)
    {
      left = std::min(left, (int)(int16_t)cd._x);
      top = std::min(top, (int)(int16_t)cd._y);
    }
    const wxPoint offset = toPixels((WORD)std::max(0, 7 - left), (WORD)std::max(0, 7 - top));

    int right = 0, bottom = 0;
    for (size_t i = 0; i < _widgets.size(); ++i)
    {
      const auto& cd = _dialog._controlData[i];
      wxWindow* widget = _widgets[i];
      if (widget == nullptr)
        continue;

      const WORD h = (cd._type == Dialog::kEditbox && cd._lines > 1) ? cd._h * cd._lines : cd._h;
      const wxPoint position = toPixels(cd._x, cd._y);
      const wxPoint size = toPixels(cd._w, h);
      wxRect rect(position.x + offset.x, position.y + offset.y, size.x, size.y);

      // native controls have their own height: center them in the area they had on Windows. Labels laid out
      // for several lines wrap and start at the top.
      const bool multiLine = (cd._type == Dialog::kEditbox && cd._lines > 1) ||
                             (cd._type == Dialog::kLabel && size.y >= widget->GetBestSize().y * 2);
      if (cd._type == Dialog::kLabel && multiLine)
      {
        static_cast<wxStaticText*>(widget)->Wrap(rect.width);
      }
      else if (!multiLine)
      {
        const int height = widget->GetBestSize().y;
        rect.y += (rect.height - height) / 2;
        rect.height = height;
      }

      widget->SetSize(rect);
      right = std::max(right, rect.GetRight());
      bottom = std::max(bottom, rect.GetBottom());
    }

    const wxPoint margin = toPixels(7, 7);
    SetClientSize(right + margin.x, bottom + margin.y);
  }

  // Updates the widgets from the controls' data.
  void sync()
  {
    _syncing = true;
    for (size_t i = 0; i < _widgets.size(); ++i)
    {
      const auto& cd = _dialog._controlData[i];
      wxWindow* widget = _widgets[i];
      if (widget == nullptr)
        continue;

      widget->Show(cd._visible);
      widget->Enable(cd._enabled);

      const wxString caption = wxString::FromUTF8(cd._caption);
      switch (cd._type)
      {
        case Dialog::kLabel:
        case Dialog::kButton:
          if (widget->GetLabel() != caption)
            widget->SetLabel(caption);
          break;

        case Dialog::kCheckbox:
        {
          auto* checkbox = static_cast<wxCheckBox*>(widget);
          if (checkbox->GetLabel() != caption)
            checkbox->SetLabel(caption);
          if (checkbox->GetValue() != cd._checkedValue)
            checkbox->SetValue(cd._checkedValue);
          break;
        }

        case Dialog::kCombobox:
        {
          auto* choice = static_cast<wxChoice*>(widget);
          bool same = choice->GetCount() == cd._options.size();
          for (unsigned j = 0; same && j < choice->GetCount(); ++j)
            same = choice->GetString(j) == wxString::FromUTF8(cd._options[j]);
          if (!same)
          {
            wxArrayString options;
            for (const auto& option : cd._options)
              options.Add(wxString::FromUTF8(option));
            choice->Set(options);
          }
          if (choice->GetSelection() != cd._selectedValue)
            choice->SetSelection(cd._selectedValue);
          break;
        }

        case Dialog::kEditbox:
        {
          auto* text = static_cast<wxTextCtrl*>(widget);
          wxString value = wxString::FromUTF8(cd._text);
          value.Replace("\r\n", "\n"); // Win32 edit controls need CRLF line ends
          if (text->GetValue() != value)
            text->ChangeValue(value);
          break;
        }
      }
    }
    _syncing = false;
  }

  void afterHook()
  {
    if (_dialog._closed)
    {
      _timer.Stop();
      if (IsModal())
        EndModal(wxID_OK);
      return;
    }

    sync();
  }

  void click(DWORD id)
  {
    if (id == IDOK)
    {
      _dialog.retrieveData();
      _dialog.close(IDOK);
    }
    else if (id == IDCANCEL)
    {
      _dialog.close(IDCANCEL);
    }
    else
    {
      _dialog.onButton(id);
    }

    afterHook();
  }

  void onCharHook(wxKeyEvent& event)
  {
    SDL_Event sdlEvent;
    if (hostwx::toSdlKeyEvent(event, true, &sdlEvent) && _dialog.onEvent(sdlEvent))
    {
      afterHook();
      return;
    }

    if (event.GetKeyCode() == WXK_ESCAPE)
    {
      click(IDCANCEL);
      return;
    }

    event.Skip();
  }

  Dialog& _dialog;
  std::vector<wxWindow*> _widgets;
  wxTimer _timer;
  bool _syncing = false;
};

bool Dialog::show()
{
  // the dialog belongs to the main thread; the game waits for it, like for a Win32 modal dialog
  if (!host::isMainThread())
  {
    bool updated = false;
    host::runOnMainThread([this, &updated]() { updated = show(); }, true);
    return updated;
  }

  extern Application app;
  extern HWND g_mainWindow;
  const bool hasBackgroundInput = app.config().getBackgroundInput();

  if (hasBackgroundInput) /* disable background input while dialog is open */
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "0");

  _updated = false;
  _closed = false;
  _result = 0;
  initControls();

  {
    DialogWindow window(*this, static_cast<wxWindow*>(g_mainWindow));
    window.CentreOnParent();
    window.ShowModal();
  }

  markClosed();

  if (hasBackgroundInput)
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

  return _updated;
}
