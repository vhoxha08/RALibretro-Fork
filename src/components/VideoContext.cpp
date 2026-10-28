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

#include "VideoContext.h"

#ifndef _WIN32
#include "host/Host.h"
#endif

#define TAG "[CONTEXT] "

VideoContext* VideoContext::s_instance = nullptr;
#ifndef _WIN32
std::atomic<int> VideoContext::s_swapInterval{1};
#endif

#ifdef _WIN32

bool VideoContext::init(libretro::LoggerComponent* logger, SDL_Window* window)
{
  _logger = logger;
  _window = window;

  _raContext = SDL_GL_CreateContext(_window);
  _coreContext = SDL_GL_CreateContext(_window);
  if (SDL_GL_MakeCurrent(_window, _raContext) != 0)
  {
    _logger->error(TAG "SDL_GL_MakeCurrent: %s", SDL_GetError());
    return false;
  }

  s_instance = this;
  return true;
}

void VideoContext::destroy()
{
  s_instance = nullptr;
}

void VideoContext::enableCoreContext(bool enable)
{
  if (enable)
  {
    SDL_GL_MakeCurrent(_window, _coreContext);
  }
  else
  {
    SDL_GL_MakeCurrent(_window, _raContext);
  }
}

void VideoContext::resetCoreContext() {
  SDL_GL_MakeCurrent(_window, _raContext);
  SDL_GL_DeleteContext(_coreContext);
  _coreContext = SDL_GL_CreateContext(_window);
}

void VideoContext::swapBuffers()
{
  SDL_GL_SwapWindow(_window);
}

void VideoContext::setSwapInterval(int interval)
{
  SDL_GL_SetSwapInterval(interval);
}

int VideoContext::getSwapInterval()
{
  return SDL_GL_GetSwapInterval();
}

void* VideoContext::getProcAddress(const char* symbol)
{
  return SDL_GL_GetProcAddress(symbol);
}

#else

bool VideoContext::init(libretro::LoggerComponent* logger)
{
  _logger = logger;

  _raContext = host::createGLContext(nullptr);
  _coreContext = _raContext ? host::createGLContext(_raContext) : nullptr;
  if (!_raContext || !_coreContext)
  {
    _logger->error(TAG "Could not create the OpenGL contexts");
    return false;
  }

  host::makeGLContextCurrent(_raContext);
  _coreCurrent = false;
  s_instance = this;
  return true;
}

void VideoContext::destroy()
{
  host::destroyGLContext(_coreContext);
  host::destroyGLContext(_raContext);
  _coreContext = _raContext = nullptr;
  s_instance = nullptr;
}

void VideoContext::enableCoreContext(bool enable)
{
  host::makeGLContextCurrent(enable ? _coreContext : _raContext);
  _coreCurrent = enable;
}

void VideoContext::resetCoreContext()
{
  host::makeGLContextCurrent(_raContext);
  host::destroyGLContext(_coreContext);
  _coreContext = host::createGLContext(_raContext);
  _coreCurrent = false;
}

void VideoContext::swapBuffers()
{
  host::swapBuffers();
}

void VideoContext::setSwapInterval(int interval)
{
  // either context may be current when the buffers are swapped
  if (!s_instance)
    return;

  s_swapInterval = interval;
  const bool coreCurrent = s_instance->_coreCurrent;
  host::makeGLContextCurrent(s_instance->_raContext);
  host::setSwapInterval(interval);
  host::makeGLContextCurrent(s_instance->_coreContext);
  host::setSwapInterval(interval);
  s_instance->enableCoreContext(coreCurrent);
}

int VideoContext::getSwapInterval()
{
  return s_swapInterval;
}

void* VideoContext::getProcAddress(const char* symbol)
{
  return host::getGLProcAddress(symbol);
}

#endif
