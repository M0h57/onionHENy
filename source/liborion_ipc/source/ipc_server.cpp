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

#include <orion/ipc_server.hpp>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <pthread.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace orion {
namespace {

IpcServerLogFn g_log = nullptr;

void vlogf(const char *fmt, va_list ap) {
  char buf[DAEMON_BUFF_MAX];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  if (g_log) {
    g_log(buf);
  }
}

void logf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vlogf(fmt, ap);
  va_end(ap);
}

struct ClientThreadArgs {
  IpcClientArgs *client;
  IpcCommandHandler handler;
  const char *tag;
};

void *client_thread(void *arg) {
  auto *pack = static_cast<ClientThreadArgs *>(arg);
  IpcClientArgs *client = pack->client;
  IpcCommandHandler handler = pack->handler;
  const char *tag = pack->tag ? pack->tag : "ipc";
  delete pack;

  logf("[%s] Thread created for Socket %i", tag, client->socket);

  IPCMessage ipcMessage{};
  int readSize = 0;
  while ((readSize = ipc_network_recv(client->socket, &ipcMessage,
                                      sizeof(ipcMessage))) > 0) {
    if (ipcMessage.magic == static_cast<int>(0xDEADBABE)) {
      std::string message = ipcMessage.msg;
      if (handler) {
        handler(client, message, ipcMessage.cmd);
      }
    } else {
      logf("[%s][client %i] Invalid magic number", tag, client->cl_nmb);
      ipcMessage.error = -1;
      ipc_network_send(client->socket, &ipcMessage, sizeof(ipcMessage));
    }
  }

  logf("[%s][client %i] IPC Connection disconnected, Shutting down ...", tag,
       client->cl_nmb);
  ipc_network_close(client->socket);
  delete client;
  return nullptr;
}

} // namespace

void ipc_server_set_log(IpcServerLogFn fn) { g_log = fn; }

int ipc_network_listen(const char *soc_path) {
  if (!soc_path) {
    return INVAIL;
  }
  unlink(soc_path);
  logf("[ipc] Deleted Socket %s", soc_path);

  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s < 0) {
    logf("[ipc] Socket failed! %s", strerror(errno));
    return INVAIL;
  }

  sockaddr_un server{};
  server.sun_family = AF_UNIX;
  strncpy(server.sun_path, soc_path, sizeof(server.sun_path) - 1);

  if (bind(s, reinterpret_cast<struct sockaddr *>(&server), SUN_LEN(&server)) <
      0) {
    logf("[ipc] Bind failed! %s", strerror(errno));
    close(s);
    return INVAIL;
  }

  if (listen(s, 100) < 0) {
    logf("[ipc] listen failed! %s", strerror(errno));
    close(s);
    return INVAIL;
  }
  return s;
}

int ipc_network_accept(int socket_fd) { return accept(socket_fd, nullptr, nullptr); }

int ipc_network_recv(int socket_fd, void *buffer, int32_t size) {
  int n = recv(socket_fd, buffer, size, 0);
  logf("got %i bytes", n);
  return n;
}

int ipc_network_send(int socket_fd, void *buffer, int32_t size) {
  return send(socket_fd, buffer, size, MSG_NOSIGNAL);
}

int ipc_network_close(int socket_fd) { return close(socket_fd); }

void ipc_reply(int sender_socket, DaemonCommands reply_cmd, bool error,
               const std::string &out_var) {
  std::string body = "{\"res\":" + std::to_string(error ? -1 : 0) +
                     ", \"var\":\"" + out_var + "\"}";

  IPCMessage outputMessage{};
  outputMessage.magic = 0xDEADBABE;
  outputMessage.cmd = reply_cmd;
  outputMessage.error = error ? -1 : 0;
  bzero(outputMessage.msg, sizeof(outputMessage.msg));
  strncpy(outputMessage.msg, body.c_str(), sizeof(outputMessage.msg) - 1);
  outputMessage.msg[sizeof(outputMessage.msg) - 1] = '\0';

  logf("error: %d", outputMessage.error);
  ipc_network_send(sender_socket, &outputMessage, sizeof(outputMessage));
}

void *ipc_server_loop(void *options_ptr) {
  auto *opts = static_cast<IpcServerOptions *>(options_ptr);
  if (!opts || !opts->socket_path || !opts->handler) {
    logf("[ipc] invalid server options");
    return nullptr;
  }

  const char *tag = opts->tag ? opts->tag : "ipc";
  int serverSocket = ipc_network_listen(opts->socket_path);
  if (serverSocket < 0) {
    logf("[%s] networkListen error %s", tag, strerror(errno));
    return nullptr;
  }

  int cli_new = 0;
  while (true) {
    int clientSocket = ipc_network_accept(serverSocket);
    if (clientSocket < 0) {
      logf("[%s] networkAccept error %s", tag, strerror(errno));
      break;
    }

    logf("[%s] Connection Accepted cl_nmb %i", tag, cli_new);

    auto *client = new IpcClientArgs();
    client->ip = "localhost";
    client->socket = clientSocket;
    client->cl_nmb = cli_new;

    auto *pack = new ClientThreadArgs{client, opts->handler, tag};
    pthread_t thr{};
    if (pthread_create(&thr, nullptr, client_thread, pack) != 0) {
      logf("[%s] pthread_create failed", tag);
      ipc_network_close(clientSocket);
      delete client;
      delete pack;
      continue;
    }
    if (opts->detach_clients) {
      pthread_detach(thr);
    }
    cli_new++;
  }

  ipc_network_close(serverSocket);
  return nullptr;
}

} // namespace orion
