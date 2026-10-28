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

#include "Host.h"

#include "Platform.h"
#include "resource.h"

#include <SDL.h>

#include <wx/app.h>
#include <wx/apptrait.h>
#include <wx/dcclient.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/frame.h>
#include <wx/glcanvas.h>
#include <wx/init.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>

#include <climits>
#include <cstdio>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

#ifdef __APPLE__
namespace hostmac
{
  void prepareGLView(void* nsView);
  void activateApplication();
  void setSwapInterval(int interval);
  int  getSwapInterval();
  void* getGLProcAddress(const char* name);
}
#endif

namespace
{
  std::function<void(const char*)> s_textInputHandler;

  class HostApp : public wxApp
  {
  public:
    // RALibretro parses its own command line
    bool OnInit() override { return true; }
  };

  host::MenuProvider* s_menuProvider = nullptr;
  wxEventLoopBase* s_eventLoop = nullptr;
  std::unique_ptr<wxEventLoopActivator> s_eventLoopActivator;

  // ===== input as SDL events =====

  Uint16 modifiers(const wxKeyboardState& state)
  {
    Uint16 mod = 0;
    if (state.ShiftDown())
      mod |= KMOD_LSHIFT;
    if (state.AltDown())
      mod |= KMOD_LALT;
    if (state.RawControlDown())
      mod |= KMOD_LCTRL;
#ifdef __APPLE__
    if (state.ControlDown()) // the Command key
      mod |= KMOD_LGUI;
#endif
    return mod;
  }

  SDL_Keycode toSdlKeycode(const wxKeyEvent& event)
  {
    const int key = event.GetKeyCode();
    if (key >= 'A' && key <= 'Z')
      return key - 'A' + 'a';
    if (key > 32 && key < 127)
      return key;

    switch (key)
    {
      case WXK_BACK: return SDLK_BACKSPACE;
      case WXK_TAB: return SDLK_TAB;
      case WXK_RETURN: return SDLK_RETURN;
      case WXK_ESCAPE: return SDLK_ESCAPE;
      case WXK_SPACE: return SDLK_SPACE;
      case WXK_DELETE: return SDLK_DELETE;
      case WXK_INSERT: return SDLK_INSERT;
      case WXK_HOME: return SDLK_HOME;
      case WXK_END: return SDLK_END;
      case WXK_PAGEUP: return SDLK_PAGEUP;
      case WXK_PAGEDOWN: return SDLK_PAGEDOWN;
      case WXK_LEFT: return SDLK_LEFT;
      case WXK_RIGHT: return SDLK_RIGHT;
      case WXK_UP: return SDLK_UP;
      case WXK_DOWN: return SDLK_DOWN;
      case WXK_SHIFT: return SDLK_LSHIFT;
      case WXK_ALT: return SDLK_LALT;
#ifdef __APPLE__
      case WXK_CONTROL: return SDLK_LGUI; // the Command key
      case WXK_RAW_CONTROL: return SDLK_LCTRL;
#else
      case WXK_CONTROL: return SDLK_LCTRL;
#endif
      case WXK_CAPITAL: return SDLK_CAPSLOCK;
      case WXK_NUMLOCK: return SDLK_NUMLOCKCLEAR;
      case WXK_SCROLL: return SDLK_SCROLLLOCK;
      case WXK_PAUSE: return SDLK_PAUSE;
      case WXK_PRINT: return SDLK_PRINTSCREEN;
      case WXK_NUMPAD0: return SDLK_KP_0;
      case WXK_NUMPAD1: return SDLK_KP_1;
      case WXK_NUMPAD2: return SDLK_KP_2;
      case WXK_NUMPAD3: return SDLK_KP_3;
      case WXK_NUMPAD4: return SDLK_KP_4;
      case WXK_NUMPAD5: return SDLK_KP_5;
      case WXK_NUMPAD6: return SDLK_KP_6;
      case WXK_NUMPAD7: return SDLK_KP_7;
      case WXK_NUMPAD8: return SDLK_KP_8;
      case WXK_NUMPAD9: return SDLK_KP_9;
      case WXK_NUMPAD_ADD: return SDLK_KP_PLUS;
      case WXK_NUMPAD_SUBTRACT: return SDLK_KP_MINUS;
      case WXK_NUMPAD_MULTIPLY: return SDLK_KP_MULTIPLY;
      case WXK_NUMPAD_DIVIDE: return SDLK_KP_DIVIDE;
      case WXK_NUMPAD_DECIMAL: return SDLK_KP_PERIOD;
      case WXK_NUMPAD_ENTER: return SDLK_KP_ENTER;
      case WXK_NUMPAD_EQUAL: return SDLK_KP_EQUALS;
      default:
        break;
    }

    if (key >= WXK_F1 && key <= WXK_F12)
      return SDLK_F1 + (key - WXK_F1);
    if (key >= WXK_F13 && key <= WXK_F24)
      return SDLK_F13 + (key - WXK_F13);

    return SDLK_UNKNOWN;
  }

  void pushKeyEvent(const wxKeyEvent& event, bool pressed)
  {
    const SDL_Keycode sym = toSdlKeycode(event);
    if (sym == SDLK_UNKNOWN)
      return;

    SDL_Event sdlEvent = {};
    sdlEvent.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    sdlEvent.key.timestamp = SDL_GetTicks();
    sdlEvent.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    sdlEvent.key.repeat = event.IsAutoRepeat() ? 1 : 0;
    sdlEvent.key.keysym.sym = sym;
    sdlEvent.key.keysym.scancode = SDL_GetScancodeFromKey(sym);
    sdlEvent.key.keysym.mod = modifiers(event);
    SDL_PushEvent(&sdlEvent);
  }

  void pushWindowEvent(Uint8 type, int data1 = 0, int data2 = 0)
  {
    SDL_Event sdlEvent = {};
    sdlEvent.type = SDL_WINDOWEVENT;
    sdlEvent.window.timestamp = SDL_GetTicks();
    sdlEvent.window.event = type;
    sdlEvent.window.data1 = data1;
    sdlEvent.window.data2 = data2;
    SDL_PushEvent(&sdlEvent);
  }

  // ===== the game view =====

  class GameCanvas : public wxGLCanvas
  {
  public:
    GameCanvas(wxWindow* parent, const wxGLAttributes& attributes) :
      wxGLCanvas(parent, attributes, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
    {
#ifdef __APPLE__
      hostmac::prepareGLView(GetHandle());
#endif

      Bind(wxEVT_KEY_DOWN, [](wxKeyEvent& event) {
        pushKeyEvent(event, true);
        event.Skip(); // generates the wxEVT_CHAR for text input
      });
      Bind(wxEVT_KEY_UP, [](wxKeyEvent& event) { pushKeyEvent(event, false); });
      Bind(wxEVT_CHAR, [](wxKeyEvent& event) {
        const wxChar c = event.GetUnicodeKey();
        if (c >= 32 && c != 127 && !event.RawControlDown() && !event.CmdDown())
        {
          if (s_textInputHandler)
            s_textInputHandler(wxString(c).utf8_str().data());
        }
      });

      Bind(wxEVT_MOTION, &GameCanvas::onMouseMove, this);
      for (const auto type : {wxEVT_LEFT_DOWN, wxEVT_LEFT_DCLICK, wxEVT_MIDDLE_DOWN, wxEVT_MIDDLE_DCLICK,
                              wxEVT_RIGHT_DOWN, wxEVT_RIGHT_DCLICK, wxEVT_LEFT_UP, wxEVT_MIDDLE_UP, wxEVT_RIGHT_UP})
      {
        Bind(type, &GameCanvas::onMouseButton, this);
      }
      Bind(wxEVT_MOUSEWHEEL, [](wxMouseEvent& event) {
        SDL_Event sdlEvent = {};
        sdlEvent.type = SDL_MOUSEWHEEL;
        sdlEvent.wheel.timestamp = SDL_GetTicks();
        const float amount = (float)event.GetWheelRotation() / (float)std::max(1, event.GetWheelDelta());
        if (event.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL)
        {
          sdlEvent.wheel.preciseX = amount;
          sdlEvent.wheel.x = (Sint32)amount;
        }
        else
        {
          sdlEvent.wheel.preciseY = amount;
          sdlEvent.wheel.y = (Sint32)amount;
        }
        SDL_PushEvent(&sdlEvent);
      });

      Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        const wxSize size = GetClientSize();
        pushWindowEvent(SDL_WINDOWEVENT_SIZE_CHANGED, size.GetWidth(), size.GetHeight());
        event.Skip();
      });
      Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxPaintDC dc(this); // the game redraws continuously; just validate the area
      });
      Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
    }

  private:
    void onMouseMove(wxMouseEvent& event)
    {
      SDL_Event sdlEvent = {};
      sdlEvent.type = SDL_MOUSEMOTION;
      sdlEvent.motion.timestamp = SDL_GetTicks();
      sdlEvent.motion.x = event.GetX();
      sdlEvent.motion.y = event.GetY();
      sdlEvent.motion.xrel = event.GetX() - _lastX;
      sdlEvent.motion.yrel = event.GetY() - _lastY;
      sdlEvent.motion.state = (event.LeftIsDown() ? SDL_BUTTON_LMASK : 0) |
                              (event.MiddleIsDown() ? SDL_BUTTON_MMASK : 0) |
                              (event.RightIsDown() ? SDL_BUTTON_RMASK : 0);
      _lastX = event.GetX();
      _lastY = event.GetY();
      SDL_PushEvent(&sdlEvent);
      event.Skip();
    }

    void onMouseButton(wxMouseEvent& event)
    {
      if (event.ButtonDown() || event.ButtonDClick())
        SetFocus();

      SDL_Event sdlEvent = {};
      const bool pressed = event.ButtonDown() || event.ButtonDClick();
      sdlEvent.type = pressed ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
      sdlEvent.button.timestamp = SDL_GetTicks();
      sdlEvent.button.state = pressed ? SDL_PRESSED : SDL_RELEASED;
      sdlEvent.button.clicks = event.ButtonDClick() ? 2 : 1;
      sdlEvent.button.x = event.GetX();
      sdlEvent.button.y = event.GetY();
      switch (event.GetButton())
      {
        case wxMOUSE_BTN_LEFT: sdlEvent.button.button = SDL_BUTTON_LEFT; break;
        case wxMOUSE_BTN_MIDDLE: sdlEvent.button.button = SDL_BUTTON_MIDDLE; break;
        case wxMOUSE_BTN_RIGHT: sdlEvent.button.button = SDL_BUTTON_RIGHT; break;
        default: event.Skip(); return;
      }
      SDL_PushEvent(&sdlEvent);
      event.Skip();
    }

    int _lastX = 0;
    int _lastY = 0;
  };

  // ===== menus =====

  // wx limits item ids to 0..32766 (the toolkit's are larger), so each command gets its own wx id
  std::unordered_map<unsigned, int> s_wxIds;
  std::unordered_map<int, unsigned> s_commandIds;

  int toWxId(unsigned id)
  {
    // macOS moves these to the application menu
    if (id == IDM_EXIT)
      return wxID_EXIT;
    if (id == IDM_ABOUT)
      return wxID_ABOUT;

    const auto it = s_wxIds.find(id);
    if (it != s_wxIds.end())
      return it->second;

    const int wxId = wxID_HIGHEST + 1 + (int)s_wxIds.size();
    s_wxIds.emplace(id, wxId);
    s_commandIds.emplace(wxId, id);
    return wxId;
  }

  unsigned fromWxId(int id)
  {
    if (id == wxID_EXIT)
      return IDM_EXIT;
    if (id == wxID_ABOUT)
      return IDM_ABOUT;

    const auto it = s_commandIds.find(id);
    return (it != s_commandIds.end()) ? it->second : 0;
  }

  class WxMenuSink : public host::MenuSink
  {
  public:
    explicit WxMenuSink(wxMenu* menu) { _menus.push_back({menu, wxString()}); }

    void beginMenu(const std::string& label) override { _menus.push_back({new wxMenu(), wxString::FromUTF8(label)}); }

    void endMenu() override
    {
      if (_menus.size() < 2)
        return;

      auto submenu = _menus.back();
      _menus.pop_back();
      _menus.back().menu->AppendSubMenu(submenu.menu, submenu.label);
    }

    void item(const std::string& label, unsigned id, bool enabled, bool checked) override
    {
      wxMenu* menu = _menus.back().menu;
      wxMenuItem* item = checked ? menu->AppendCheckItem(toWxId(id), wxString::FromUTF8(label))
                                 : menu->Append(toWxId(id), wxString::FromUTF8(label));
      if (checked)
        item->Check(true);
      item->Enable(enabled);
    }

    void separator() override { _menus.back().menu->AppendSeparator(); }

  private:
    struct Level
    {
      wxMenu* menu;
      wxString label;
    };
    std::vector<Level> _menus;
  };

  class MainFrame : public wxFrame
  {
  public:
    explicit MainFrame(const wxString& title) : wxFrame(nullptr, wxID_ANY, title)
    {
      wxGLAttributes attributes;
      attributes.PlatformDefaults().RGBA().MinRGBA(8, 8, 8, 8).DoubleBuffer().EndList();
      _canvas = new GameCanvas(this, attributes);

      auto* menuBar = new wxMenuBar();
      _fileMenu = new wxMenu();
      _settingsMenu = new wxMenu();
      _raMenu = new wxMenu();
      _helpMenu = new wxMenu();
      menuBar->Append(_fileMenu, "&File");
      menuBar->Append(_settingsMenu, "&Settings");
      menuBar->Append(_raMenu, "&RetroAchievements");
      menuBar->Append(_helpMenu, "&Help");
      SetMenuBar(menuBar);

      Bind(wxEVT_MENU_OPEN, [this](wxMenuEvent& event) {
        if (wxMenu* menu = event.GetMenu())
          rebuild(menu);
        event.Skip();
      });
      Bind(wxEVT_MENU, [](wxCommandEvent& event) {
        const unsigned id = fromWxId(event.GetId());
        if (s_menuProvider && id != 0)
          s_menuProvider->onMenuCommand(id);
        else
          event.Skip();
      });
      Bind(wxEVT_CLOSE_WINDOW, [](wxCloseEvent& event) {
        // the application quits through its own loop (and destroys this window)
        if (event.CanVeto())
          event.Veto();
        SDL_Event quit = {};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
      });
      Bind(wxEVT_ACTIVATE, [](wxActivateEvent& event) {
        pushWindowEvent(event.GetActive() ? SDL_WINDOWEVENT_FOCUS_GAINED : SDL_WINDOWEVENT_FOCUS_LOST);
        event.Skip();
      });
    }

    GameCanvas* canvas() const { return _canvas; }

    void rebuildAll()
    {
      for (wxMenu* menu : {_fileMenu, _settingsMenu, _raMenu, _helpMenu})
        rebuild(menu);
    }

  private:
    void rebuild(wxMenu* menu)
    {
      host::Menu which;
      if (menu == _fileMenu)
        which = host::Menu::File;
      else if (menu == _settingsMenu)
        which = host::Menu::Settings;
      else if (menu == _raMenu)
        which = host::Menu::RetroAchievements;
      else if (menu == _helpMenu)
        which = host::Menu::Help;
      else
        return; // submenus are rebuilt with their top level menu

      if (!s_menuProvider)
        return;

      while (menu->GetMenuItemCount() > 0)
        menu->Destroy(menu->FindItemByPosition(0));

      WxMenuSink sink(menu);
      s_menuProvider->buildMenu(which, sink);
    }

    GameCanvas* _canvas;
    wxMenu* _fileMenu;
    wxMenu* _settingsMenu;
    wxMenu* _raMenu;
    wxMenu* _helpMenu;
  };

  MainFrame* s_frame = nullptr;

  wxGLContextAttrs& contextAttributes()
  {
    static wxGLContextAttrs s_attributes;
    static bool s_initialized = false;
    if (!s_initialized)
    {
      // the frontend's shaders (and ImGui's) are "#version 150": OpenGL 3.2 core
      s_attributes.PlatformDefaults().CoreProfile().OGLVersion(3, 2).EndList();
      s_initialized = true;
    }
    return s_attributes;
  }
}

// ===== lifecycle =====

bool host::init(int argc, char* argv[])
{
  wxApp::SetInstance(new HostApp());
  if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
    return false;

  // a failed assertion is logged instead of blocking the game with a dialog
  wxSetAssertHandler([](const wxString& file, int line, const wxString& func, const wxString& cond, const wxString& msg)
  {
    fprintf(stderr, "%s(%d): assert \"%s\" failed in %s(): %s\n", (const char*)file.utf8_str(), line,
            (const char*)cond.utf8_str(), (const char*)func.utf8_str(), (const char*)msg.utf8_str());
  });

  // RALibretro runs its own loop and handles the native events from it (pumpEvents)
  s_eventLoop = wxTheApp->GetTraits()->CreateEventLoop();
  s_eventLoopActivator.reset(new wxEventLoopActivator(s_eventLoop));
  return true;
}

void host::shutdown()
{
  s_eventLoopActivator.reset();
  delete s_eventLoop;
  s_eventLoop = nullptr;

  if (wxTheApp)
  {
    wxTheApp->OnExit();
    wxEntryCleanup();
  }
}

void host::setTextInputHandler(std::function<void(const char*)> handler)
{
  s_textInputHandler = std::move(handler);
}

void host::pumpEvents()
{
  if (!s_eventLoop)
    return;

  // Pending() isn't reliable on macOS (Dispatch() may then block), so poll with a zero timeout. Bounded so a
  // stream of native events can't starve the game.
  for (int i = 0; i < 64 && s_eventLoop->DispatchTimeout(0) > 0; ++i)
    continue;

  wxTheApp->ProcessPendingEvents();
  wxTheApp->ProcessIdle();
}

// ===== main window =====

void* host::createMainWindow(const char* title, int x, int y, int width, int height)
{
  s_frame = new MainFrame(wxString::FromUTF8(title));
  s_frame->SetClientSize(width, height);
  if (x == INT_MIN || y == INT_MIN || wxDisplay::GetFromPoint(wxPoint(x + 40, y + 10)) == wxNOT_FOUND)
    s_frame->Centre();
  else
    s_frame->Move(x, y);

  s_frame->rebuildAll();
  s_frame->Show();
  s_frame->canvas()->SetFocus();
#ifdef __APPLE__
  hostmac::activateApplication();
#endif

  // let the window appear before the GL contexts attach to it
  pumpEvents();
  return s_frame;
}

void host::destroyMainWindow()
{
  if (!s_frame)
    return;

  s_frame->Destroy();
  s_frame = nullptr;
  if (wxTheApp)
    wxTheApp->ProcessIdle(); // performs the delayed destruction
}

void host::getWindowPosition(int* x, int* y)
{
  const wxPoint position = s_frame ? s_frame->GetPosition() : wxPoint(0, 0);
  *x = position.x;
  *y = position.y;
}

void host::getClientSize(int* width, int* height)
{
  const wxSize size = s_frame ? s_frame->canvas()->GetClientSize() : wxSize(0, 0);
  *width = size.GetWidth();
  *height = size.GetHeight();
}

void host::setClientSize(int width, int height)
{
  if (s_frame)
    s_frame->SetClientSize(width, height);
}

void host::setTitle(const std::string& title)
{
  if (s_frame)
    s_frame->SetTitle(wxString::FromUTF8(title));
}

bool host::isFullscreen()
{
  return s_frame && s_frame->IsFullScreen();
}

void host::setFullscreen(bool fullscreen)
{
  if (s_frame)
    s_frame->ShowFullScreen(fullscreen);
}

void host::showCursor(bool show)
{
  if (s_frame)
    s_frame->canvas()->SetCursor(show ? wxNullCursor : wxCursor(wxCURSOR_BLANK));
}

// ===== menu bar =====

void host::setMenuProvider(MenuProvider* provider)
{
  s_menuProvider = provider;
  if (s_frame)
    s_frame->rebuildAll();
}

// ===== OpenGL =====

int host::messageBox(const char* text, const char* caption, unsigned type)
{
  long style = wxCENTRE;
  if ((type & MB_ICONWARNING) == MB_ICONWARNING)
    style |= wxICON_WARNING;
  else if (type & MB_ICONERROR)
    style |= wxICON_ERROR;
  else
    style |= wxICON_INFORMATION;

  if (type & MB_YESNO)
    style |= wxYES_NO;
  else if (type & MB_OKCANCEL)
    style |= wxOK | wxCANCEL;
  else
    style |= wxOK;

  wxMessageDialog dialog(s_frame, wxString::FromUTF8(text), wxString::FromUTF8(caption), style);
  switch (dialog.ShowModal())
  {
    case wxID_YES:    return IDYES;
    case wxID_NO:     return IDNO;
    case wxID_CANCEL: return IDCANCEL;
    default:          return IDOK;
  }
}

void host::setClipboardText(const char* utf8Text)
{
  wxClipboardLocker locker;
  if (!!locker)
    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(utf8Text)));
}

std::string host::getClipboardText()
{
  wxClipboardLocker locker;
  if (!locker || !wxTheClipboard->IsSupported(wxDF_UNICODETEXT))
    return std::string();

  wxTextDataObject data;
  wxTheClipboard->GetData(data);
  return std::string(data.GetText().utf8_str());
}

void* host::createGLContext(void* shareWith)
{
  if (!s_frame)
    return nullptr;

  auto* context = new wxGLContext(s_frame->canvas(), static_cast<wxGLContext*>(shareWith), &contextAttributes());
  if (!context->IsOK())
  {
    delete context;
    return nullptr;
  }
  return context;
}

void host::destroyGLContext(void* context)
{
  delete static_cast<wxGLContext*>(context);
}

void host::makeGLContextCurrent(void* context)
{
  if (s_frame && context)
    static_cast<wxGLContext*>(context)->SetCurrent(*s_frame->canvas());
}

void host::swapBuffers()
{
  if (s_frame)
    s_frame->canvas()->SwapBuffers();
}

void host::setSwapInterval(int interval)
{
#ifdef __APPLE__
  hostmac::setSwapInterval(interval);
#else
  (void)interval;
#endif
}

int host::getSwapInterval()
{
#ifdef __APPLE__
  return hostmac::getSwapInterval();
#else
  return 0;
#endif
}

void* host::getGLProcAddress(const char* name)
{
#ifdef __APPLE__
  return hostmac::getGLProcAddress(name);
#else
  (void)name;
  return nullptr;
#endif
}

int host::displayRefreshRate()
{
  if (!s_frame)
    return 0;

  const int index = wxDisplay::GetFromWindow(s_frame);
  if (index == wxNOT_FOUND)
    return 0;

  return wxDisplay((unsigned)index).GetCurrentMode().refresh;
}
