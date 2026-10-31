/*
Copyright (C) 2026 RALibretro contributors

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

// (before anything includes windows.h)
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include "Pine.h"

#include <stdlib.h>
#include <string.h>

#define TAG "[PINE] "

namespace
{
  // the protocol's commands (pine.h)
  enum Command : uint8_t
  {
    MsgRead8 = 0,
    MsgRead16 = 1,
    MsgRead32 = 2,
    MsgRead64 = 3,
    MsgWrite8 = 4,
    MsgWrite16 = 5,
    MsgWrite32 = 6,
    MsgWrite64 = 7,
    MsgVersion = 8,
    MsgSaveState = 9,
    MsgLoadState = 10,
    MsgTitle = 11,
    MsgID = 12,
    MsgUUID = 13,
    MsgGameVersion = 14,
    MsgStatus = 15
  };

  const uint8_t kOk = 0;
  const uint8_t kFail = 0xff;

  const uint32_t kMaxRequestSize = 650000;
  const uint32_t kMaxReplySize = 450000;

  const size_t kMaxClients = 8;

#ifdef _WIN32
  bool wouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
  void closeSocket(intptr_t socket) { closesocket((SOCKET)socket); }
  int lastError() { return WSAGetLastError(); }

  bool setNonBlocking(intptr_t socket)
  {
    u_long on = 1;
    return ioctlsocket((SOCKET)socket, FIONBIO, &on) == 0;
  }

  bool waitWritable(intptr_t socket, int milliseconds)
  {
    fd_set set;
    FD_ZERO(&set);
    FD_SET((SOCKET)socket, &set);
    timeval timeout = {milliseconds / 1000, (milliseconds % 1000) * 1000};
    return select(0, NULL, &set, NULL, &timeout) == 1;
  }

  const int kSendFlags = 0;
#else
  bool wouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR; }
  void closeSocket(intptr_t socket) { ::close((int)socket); }
  int lastError() { return errno; }

  bool setNonBlocking(intptr_t socket)
  {
    const int flags = fcntl((int)socket, F_GETFL, 0);
    return flags != -1 && fcntl((int)socket, F_SETFL, flags | O_NONBLOCK) == 0;
  }

  bool waitWritable(intptr_t socket, int milliseconds)
  {
    pollfd fd = {(int)socket, POLLOUT, 0};
    return ::poll(&fd, 1, milliseconds) == 1;
  }

#ifdef MSG_NOSIGNAL
  const int kSendFlags = MSG_NOSIGNAL; // a closed connection isn't a SIGPIPE (Linux)
#else
  const int kSendFlags = 0;            // (macOS: SO_NOSIGPIPE)
#endif
#endif

  // the protocol is little endian, like the systems RALibretro runs on
  void put32(std::vector<uint8_t>& data, uint32_t value)
  {
    uint8_t bytes[4];
    memcpy(bytes, &value, sizeof(bytes));
    data.insert(data.end(), bytes, bytes + sizeof(bytes));
  }

  // like PCSX2's: the size includes the terminating null
  void putString(std::vector<uint8_t>& data, const std::string& value)
  {
    put32(data, (uint32_t)value.size() + 1);
    data.insert(data.end(), value.begin(), value.end());
    data.push_back(0);
  }
}

Pine::Pine() : _logger(NULL), _listener(kNoSocket), _slot(0)
{
}

Pine::~Pine()
{
  stop();
}

bool Pine::start(unsigned slot, libretro::LoggerComponent* logger)
{
  stop();

  _logger = logger;
  _slot = slot;

  if (slot == 0 || slot > 65535)
  {
    _logger->error(TAG "Invalid slot %u", slot);
    return false;
  }

#ifdef _WIN32
  static bool wsaStarted = false;
  if (!wsaStarted)
  {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      _logger->error(TAG "Winsock unavailable");
      return false;
    }
    wsaStarted = true;
  }

  SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET)
  {
    _logger->error(TAG "Could not create the socket (%d)", lastError());
    return false;
  }

  // (not shared with another program listening on the port)
  BOOL exclusive = TRUE;
  setsockopt(listener, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&exclusive, sizeof(exclusive));

  sockaddr_in address;
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons((u_short)slot);

  if (bind(listener, (const sockaddr*)&address, sizeof(address)) != 0 || listen(listener, SOMAXCONN) != 0 ||
      !setNonBlocking((intptr_t)listener))
  {
    _logger->error(TAG "Could not listen on 127.0.0.1:%u (%d)", slot, lastError());
    closesocket(listener);
    return false;
  }

  _listener = (intptr_t)listener;
  _logger->info(TAG "Listening on 127.0.0.1:%u", slot);
#else
#ifdef __APPLE__
  const char* directory = getenv("TMPDIR");
#else
  const char* directory = getenv("XDG_RUNTIME_DIR");
#endif
  _path = (directory != NULL && *directory != '\0') ? directory : "/tmp";
  while (_path.size() > 1 && _path.back() == '/')
    _path.pop_back();
  _path += "/ralibretro.sock";
  if (slot != kDefaultSlot)
    _path += "." + std::to_string(slot);

  sockaddr_un address;
  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  if (_path.size() >= sizeof(address.sun_path))
  {
    _logger->error(TAG "Socket path too long: %s", _path.c_str());
    _path.clear();
    return false;
  }
  memcpy(address.sun_path, _path.c_str(), _path.size() + 1);

  // another RALibretro listening on it keeps it; one left by a RALibretro that ended is replaced
  int probe = socket(AF_UNIX, SOCK_STREAM, 0);
  if (probe != -1)
  {
    const bool inUse = connect(probe, (const sockaddr*)&address, sizeof(address)) == 0;
    ::close(probe);
    if (inUse)
    {
      _logger->error(TAG "%s is used by another program", _path.c_str());
      _path.clear();
      return false;
    }
  }
  unlink(_path.c_str());

  int listener = socket(AF_UNIX, SOCK_STREAM, 0);
  if (listener == -1 || bind(listener, (const sockaddr*)&address, sizeof(address)) != 0 ||
      listen(listener, SOMAXCONN) != 0 || !setNonBlocking(listener))
  {
    _logger->error(TAG "Could not listen on %s (%d)", _path.c_str(), lastError());
    if (listener != -1)
      ::close(listener);
    unlink(_path.c_str());
    _path.clear();
    return false;
  }

  _listener = listener;
  _logger->info(TAG "Listening on %s", _path.c_str());
#endif

  return true;
}

void Pine::stop()
{
  for (auto& client : _clients)
    close(client);
  _clients.clear();

  if (_listener != kNoSocket)
  {
    closeSocket(_listener);
    _listener = kNoSocket;
    _logger->info(TAG "Stopped");
  }

#ifndef _WIN32
  if (!_path.empty())
  {
    unlink(_path.c_str());
    _path.clear();
  }
#endif
}

void Pine::poll(Target& target)
{
  if (!isRunning())
    return;

  for (;;)
  {
    const intptr_t socket = (intptr_t)accept(_listener, NULL, NULL);
    if (socket == kNoSocket)
      break;

    if (_clients.size() >= kMaxClients || !setNonBlocking(socket))
    {
      closeSocket(socket);
      continue;
    }

#ifdef __APPLE__
    int on = 1;
    setsockopt((int)socket, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
#endif

    _clients.push_back({socket, {}});
    _logger->debug(TAG "Client connected");
  }

  for (size_t i = 0; i < _clients.size();)
  {
    Client& client = _clients[i];
    bool open = true;

    // what it sent
    for (;;)
    {
      uint8_t buffer[65536];
      const auto count = ::recv(client.socket, (char*)buffer, sizeof(buffer), 0);
      if (count > 0)
      {
        client.input.insert(client.input.end(), buffer, buffer + count);
        continue;
      }

      if (count < 0 && wouldBlock())
        break;

      open = false; // closed, or an error
      break;
    }

    // the requests it sent completely
    size_t offset = 0;
    while (open && client.input.size() - offset >= 4)
    {
      uint32_t size;
      memcpy(&size, &client.input[offset], sizeof(size));
      if (size < 5 || size > kMaxRequestSize)
      {
        _logger->warn(TAG "Invalid request size %u, closing the connection", size);
        open = false;
        break;
      }

      if (client.input.size() - offset < size)
        break;

      std::vector<uint8_t> reply;
      if (!handle(target, &client.input[offset + 4], size - 4, reply))
      {
        reply.clear();
        put32(reply, 5);
        reply.push_back(kFail);
      }

      open = send(client.socket, reply);
      offset += size;
    }

    if (open)
    {
      client.input.erase(client.input.begin(), client.input.begin() + offset);
      ++i;
    }
    else
    {
      close(client);
      _clients.erase(_clients.begin() + i);
      _logger->debug(TAG "Client disconnected");
    }
  }
}

// The commands one after the other (a batch); the reply has their results one after the other, or is a failure if
// any of them failed
bool Pine::handle(Target& target, const uint8_t* request, uint32_t size, std::vector<uint8_t>& reply)
{
  reply.clear();
  put32(reply, 0); // (the size, below)
  reply.push_back(kOk);

  uint32_t position = 0;
  while (position < size)
  {
    const uint8_t command = request[position++];
    switch (command)
    {
      case MsgRead8:
      case MsgRead16:
      case MsgRead32:
      case MsgRead64:
      {
        const uint32_t width = 1u << (command - MsgRead8);
        uint32_t address;
        if (size - position < sizeof(address))
          return false;
        memcpy(&address, request + position, sizeof(address));
        position += sizeof(address);

        uint8_t value[8];
        if (!target.read(address, value, width))
          return false;
        reply.insert(reply.end(), value, value + width);
        break;
      }

      case MsgWrite8:
      case MsgWrite16:
      case MsgWrite32:
      case MsgWrite64:
      {
        const uint32_t width = 1u << (command - MsgWrite8);
        uint32_t address;
        if (size - position < sizeof(address) + width)
          return false;
        memcpy(&address, request + position, sizeof(address));
        position += sizeof(address);

        if (!target.write(address, request + position, width))
          return false;
        position += width;
        break;
      }

      case MsgSaveState:
      case MsgLoadState:
      {
        if (size - position < 1)
          return false;
        const uint8_t slot = request[position++];

        if (!(command == MsgSaveState ? target.saveState(slot) : target.loadState(slot)))
          return false;
        break;
      }

      case MsgVersion:     putString(reply, target.version()); break;
      case MsgTitle:       putString(reply, target.title()); break;
      case MsgID:          putString(reply, target.id()); break;
      case MsgUUID:        putString(reply, target.uuid()); break;
      case MsgGameVersion: putString(reply, target.gameVersion()); break;
      case MsgStatus:      put32(reply, (uint32_t)target.status()); break;

      default:
        return false; // (MsgUnimplemented)
    }

    if (reply.size() > kMaxReplySize)
      return false;
  }

  const uint32_t replySize = (uint32_t)reply.size();
  memcpy(&reply[0], &replySize, sizeof(replySize));
  return true;
}

// All of it (a client that doesn't read it within a second is disconnected)
bool Pine::send(intptr_t socket, const std::vector<uint8_t>& data)
{
  size_t sent = 0;
  while (sent < data.size())
  {
    const auto count = ::send(socket, (const char*)data.data() + sent, (int)(data.size() - sent), kSendFlags);
    if (count > 0)
    {
      sent += (size_t)count;
      continue;
    }

    if (count < 0 && wouldBlock() && waitWritable(socket, 1000))
      continue;

    return false;
  }

  return true;
}

void Pine::close(Client& client)
{
  if (client.socket != kNoSocket)
  {
    closeSocket(client.socket);
    client.socket = kNoSocket;
  }
}
