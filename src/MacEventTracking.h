#pragma once

#include <functional>

// macOS runs a nested event loop while a window is being resized or moved, or a menu or a drop down list is
// open. The application's own loop doesn't run meanwhile, so the game would stop. This calls `step` repeatedly
// from inside those loops (only there).
void startEventTrackingTimer(std::function<void()> step);
