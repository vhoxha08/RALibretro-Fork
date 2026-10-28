#include "MacEventTracking.h"

#import <AppKit/AppKit.h>

void startEventTrackingTimer(std::function<void()> step)
{
  static std::function<void()> s_step;
  s_step = std::move(step);

  // the step blocks on the audio buffer like a normal frame, which paces it; the interval only needs to be short
  CFRunLoopTimerRef timer = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent(),
                                                            1.0 / 240.0, 0, 0, ^(CFRunLoopTimerRef) {
    // modal windows keep the game paused, like on Windows
    if ([NSApp modalWindow] == nil && s_step)
      s_step();
  });

  // only fires while macOS is tracking a resize, a move or a menu
  CFRunLoopAddTimer(CFRunLoopGetMain(), timer, (__bridge CFStringRef)NSEventTrackingRunLoopMode);
  CFRelease(timer);
}
