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

// macOS specifics of the native user interface (see Host.h)

#import <AppKit/AppKit.h>
#import <OpenGL/OpenGL.h>

#include <dlfcn.h>

namespace hostmac
{
  void prepareGLView(void* nsView)
  {
    // the frontend works in window points (like the SDL window it replaces): draw at 1x
    NSView* view = (__bridge NSView*)nsView;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [view setWantsBestResolutionOpenGLSurface:NO];
#pragma clang diagnostic pop
  }

  void activateApplication()
  {
    // started from a terminal (not an application bundle) the process isn't a regular application yet
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
  }

  void makeCurrent(void* glContext)
  {
    [(__bridge NSOpenGLContext*)glContext makeCurrentContext];
  }

  void clearCurrent()
  {
    [NSOpenGLContext clearCurrentContext];
  }

  // The view changed size: the drawable follows (main thread). Locked against the game thread's drawing.
  void updateContext(void* glContext)
  {
    NSOpenGLContext* context = (__bridge NSOpenGLContext*)glContext;
    CGLLockContext([context CGLContextObj]);
    [context update];
    CGLUnlockContext([context CGLContextObj]);
  }

  void flushCurrent()
  {
    CGLContextObj context = CGLGetCurrentContext();
    if (context)
    {
      CGLLockContext(context);
      CGLFlushDrawable(context);
      CGLUnlockContext(context);
    }
  }

  // ===== relative mouse =====

  static id s_mouseMonitor = nil;

  // Moves the cursor over the middle of the view, where the clicks reach the game.
  void centerMouse(void* nsView)
  {
    NSView* view = (__bridge NSView*)nsView;
    NSRect frame = [view.window convertRectToScreen:[view convertRect:view.bounds toView:nil]];
    // Cocoa's screen coordinates start at the bottom of the main screen, Quartz's at its top
    const CGFloat mainHeight = NSScreen.screens.firstObject.frame.size.height;
    CGWarpMouseCursorPosition(CGPointMake(NSMidX(frame), mainHeight - NSMidY(frame)));
  }

  void startRelativeMouse(void* nsView, void (*onMotion)(double dx, double dy))
  {
    if (s_mouseMonitor != nil)
      return;

    centerMouse(nsView);
    CGAssociateMouseAndMouseCursorPosition(false); // the cursor stays put, the movements are still reported
    [NSCursor hide];

    const NSEventMask mask = NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged |
                             NSEventMaskOtherMouseDragged;
    s_mouseMonitor = [[NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent* event) {
      onMotion(event.deltaX, event.deltaY);
      return event;
    }] retain];
  }

  void stopRelativeMouse()
  {
    if (s_mouseMonitor == nil)
      return;

    [NSEvent removeMonitor:s_mouseMonitor];
    [s_mouseMonitor release];
    s_mouseMonitor = nil;

    CGAssociateMouseAndMouseCursorPosition(true);
    [NSCursor unhide];
  }

  void setSwapInterval(int interval)
  {
    CGLContextObj context = CGLGetCurrentContext();
    if (context)
    {
      GLint value = interval;
      CGLSetParameter(context, kCGLCPSwapInterval, &value);
    }
  }

  int getSwapInterval()
  {
    CGLContextObj context = CGLGetCurrentContext();
    GLint value = 0;
    if (context)
      CGLGetParameter(context, kCGLCPSwapInterval, &value);
    return value;
  }

  void* getGLProcAddress(const char* name)
  {
    static void* s_library = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
    return s_library ? dlsym(s_library, name) : nullptr;
  }
}
