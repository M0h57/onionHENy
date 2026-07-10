/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * ShellUI IPC client entry — no HookedFuncs (true compile seam).
 * Call sites that need UI types/hooks include HookedFuncs.hpp separately.
 */

#pragma once

#include <orion/ipc_client.hpp>

// shellui-only IPC-adjacent globals
extern bool cheats_shortcut_activate;
