/* Copyright (C) 2025 OnionHEN / LightningMods

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

// Transitive headers historically provided by the header-only IPC client.
#include <array>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdarg.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

// Thin compatibility shim: implementation lives in libonion_ipc.
// game_log / shellui_log / IPC_Client are provided by the shared library.
#include <onion/ipc_client.hpp>

extern bool cheats_shortcut_activate;
pid_t find_pid(const char *name, bool needle, bool for_bigapp,
               bool need_eboot = false);
