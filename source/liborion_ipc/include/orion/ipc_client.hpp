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
// Shared injectee-side IPC client (crit + util daemons).
//
// Design rules:
//   - One connection object per target (crit vs util). Never flip a flag on a
//     shared singleton — that races MainDaemonSocket / UtilDaemonSocket.
//   - Implementation lives in the .cpp (not header-only).
//   - JSON string fields are built with cJSON (escape-safe).
// ---------------------------------------------------------------------------

enum Cheat_Actions {
  DOWNLOAD_CHEATS = 0,
};

// Legacy free loggers used by shellui / fps_elf (Detour, Mono, hooks).
// Both forward to the same klog sink.
// Note: no printf format attribute — existing call sites pass typed pointers
// with %p under -Wformat-pedantic / -Werror (historical header-only logger).
void shellui_log(const char *fmt, ...);
void game_log(const char *fmt, ...);
void orion_ipc_log(const char *fmt, ...);

// Optional: host may install a notify sink (e.g. shellui bubble). Default: none.
using OrionIpcNotifyFn = void (*)(const char *text);
void orion_ipc_set_notify(OrionIpcNotifyFn fn);

class IPC_Client {
public:
  IPC_Client(const IPC_Client &) = delete;
  IPC_Client &operator=(const IPC_Client &) = delete;

  // Two true singletons: crit (false) and util (true). Safe to call from
  // concurrent contexts against different targets.
  static IPC_Client &getInstance(bool is_util_daemon);

  bool is_util() const { return util_daemon_; }

  void set_recv_timeout_ms(int ms) { recv_timeout_ms_ = ms; }
  int recv_timeout_ms() const { return recv_timeout_ms_; }

  // Low-level transport
  int OpenConnection(const char *path);
  bool IPCOpenConnection();
  bool IPCOpenIfNotConnected();
  int IPCReceiveData(IPCMessage &msg, std::string &ipc_msg);
  int IPCSendData(const IPCMessage &msg);
  int IPCCloseConnection();
  bool IPCSendCommand(DaemonCommands cmd, std::string &ipc_msg1,
                      std::string ipc_msg2 = "");

  // High-level commands
  int GetDaemonPid();
  IPC_Ret ToggleSetting(DaemonCommands cmd, bool turn_on);
  IPC_Ret DownloadKstuff();
  void KillDaemon();
  void ForceKillPID(int pid);
  IPC_Ret CopyFile(std::string src, std::string dest);
  IPC_Ret LaunchPlugin(std::string plugin_path, std::string tid);
  bool GameVerFromTid(std::string tid, std::string &out_ver);
  bool Remount(const char *src, const char *dest);
  bool GetGameCheats(const std::string &tid, const std::string &ver,
                     std::string &cheats);
  bool ToggleGameCheat(int pid, const std::string &tid, int cheat_index,
                       std::string &cheat_enabled,
                       const std::string &version = "");
  void SendRestModeAction();
  void Reload_Daemon_Settings();
  bool Launch_Elfldr();
  bool Cheats_Action(Cheat_Actions act, int repo = 0);
  bool Set_Fan_Threshold(int temp, bool enabled);
  bool ToggleDPI(bool turn_on, bool is_v2);

  // Kept for call-site readability (matches historical public field).
  // Prefer is_util(); do not reassign after construction.
  const bool util_daemon;

private:
  explicit IPC_Client(bool util_daemon, int recv_timeout_ms);

  bool util_daemon_;
  int socket_fd_;
  int recv_timeout_ms_;

  const char *socket_path() const;
  bool require_util(const char *what) const;
  bool require_crit(const char *what) const;
};
