/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * ShellUI process globals — ToolboxUiState lives here.
 */

#include "shellui_state.hpp"

#include <orion/settings.hpp>

orion::Settings g_settings;
OverlayLayout g_overlay_layout;

ToolboxUiState g_ui;

bool &is_plugin = g_ui.is_plugin;
bool &is_su_menu = g_ui.is_su_menu;
bool &is_debug_settings = g_ui.is_debug_settings;
bool &is_cheats = g_ui.is_cheats;
bool &is_auto_plugin = g_ui.is_auto_plugin;
bool &is_remote_play = g_ui.is_remote_play;
bool &is_plapps = g_ui.is_plapps;
bool &cheats_shortcut_activated = g_ui.cheats_shortcut_activated;
bool &cheats_shortcut_activated_not_open = g_ui.cheats_shortcut_activated_not_open;
bool &is_game_open = g_ui.is_game_open;
bool &is_current_game_open = g_ui.is_current_game_open;
bool &g_all_cpu_usage = g_ui.all_cpu_usage;

std::string &running_tid = g_ui.running_tid;
std::string &current_menu_tid = g_ui.current_menu_tid;
std::string &currentCheatTID = g_ui.current_cheat_tid;
std::string &remote_play_info = g_ui.remote_play_info;

std::vector<GameEntry> &games_list = g_ui.games_list;
std::vector<Plugins> &plugins_list = g_ui.plugins_list;
std::vector<Plugins> &auto_list = g_ui.auto_list;
std::vector<Payloads_Apps> &payloads_apps_list = g_ui.payloads_apps_list;

int *const cheatEnabledMap = g_ui.cheat_enabled_map;

std::atomic_bool &cheat_action_in_progress = g_ui.cheat_action_in_progress;
std::atomic_bool &download_kstuff_thread_in_progress =
    g_ui.download_kstuff_thread_in_progress;
