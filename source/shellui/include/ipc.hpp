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

// Transitive headers historically provided by the header-only IPC client.
// Call sites (MonoUtils, prx, HookFunctions, Detour) still rely on them.
#include <array>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdarg.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

// Thin compatibility shim: implementation lives in liborion_ipc.
#include <orion/ipc_client.hpp>

// shellui-only helpers that historically lived next to the IPC client.
#include "HookedFuncs.hpp"

extern bool cheats_shortcut_activate;
pid_t find_pid(const char *name, bool needle, bool for_bigapp,
               bool need_eboot = false);
