/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Extracted from MonoUtils.cpp for module locality.
 */

#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include "defs.h"
#include "RemotePlay.h"
#include "external_symbols.hpp"
#include "proc.h"
#include <orion/settings.hpp>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdlib>
#include <vector>
#include <string>

orion::Settings g_settings;
OverlayLayout g_overlay_layout;
bool g_all_cpu_usage = false;

std::vector<GameEntry> games_list;
std::vector<Plugins> plugins_list, auto_list;
std::vector<Payloads_Apps> payloads_apps_list;

std::string running_tid;
bool is_game_open = true;
bool is_current_game_open = true;
int cheatEnabledMap[256];
std::string remote_play_info;

