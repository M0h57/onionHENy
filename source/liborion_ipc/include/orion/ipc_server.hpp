/* Copyright (C) 2025 OrionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#pragma once

#include <msg.hpp>
#include <string>

// ---------------------------------------------------------------------------
// Shared daemon-side Unix IPC transport.
//
// Architecture:
//   wire protocol  →  msg.hpp (IPCMessage, paths, commands)
//   transport      →  this module (listen / accept / recv / send / loop)
//   business       →  process handleIPC (daemon vs util command tables)
//
// Clients live in ipc_client.hpp (injectees). Servers must not #include the
// client implementation.
// ---------------------------------------------------------------------------

namespace orion {

struct IpcClientArgs {
  std::string ip;
  int socket = -1;
  int cl_nmb = 0;
};

using IpcCommandHandler = void (*)(IpcClientArgs *client, std::string &msg,
                                   DaemonCommands cmd);

// Log sink receives a single line (already formatted, no required trailing \n).
using IpcServerLogFn = void (*)(const char *line);
void ipc_server_set_log(IpcServerLogFn fn);

// --- transport primitives ---
int ipc_network_listen(const char *soc_path);
int ipc_network_accept(int socket_fd);
int ipc_network_recv(int socket_fd, void *buffer, int32_t size);
int ipc_network_send(int socket_fd, void *buffer, int32_t size);
int ipc_network_close(int socket_fd);

// Build {"res":N,"var":"..."} reply with the process's return command ordinal.
void ipc_reply(int sender_socket, DaemonCommands reply_cmd, bool error,
               const std::string &out_var = "Nothing");

// Server loop options (passed by pointer into pthread entry).
struct IpcServerOptions {
  const char *socket_path = nullptr;
  IpcCommandHandler handler = nullptr;
  DaemonCommands reply_cmd = BREW_RETURN_VALUE;
  bool detach_clients = true; // util always did; daemon should too (no leak)
  const char *tag = "ipc";
};

// pthread-compatible entry: arg must be IpcServerOptions* with static lifetime.
void *ipc_server_loop(void *options_ptr);

} // namespace orion

// Historical name used by daemon/util sources.
using clientArgs = orion::IpcClientArgs;
