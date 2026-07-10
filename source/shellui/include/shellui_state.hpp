/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Toolbox / settings UI runtime state — progressive consolidation of former
 * free globals (is_*, lists, cheat map). Prefer g_ui.* at new call sites.
 */
#pragma once

#include "shellui_types.hpp"

#include <atomic>
#include <string>
#include <vector>

/** Settings page / resource-stream context for ShellUI hooks. */
struct ToolboxUiState {
  // Manifest resource flags (GetManifestResourceStream_Hook)
  bool is_plugin = false;
  bool is_su_menu = false;
  bool is_debug_settings = false;
  bool is_cheats = false;
  bool is_auto_plugin = false;
  bool is_remote_play = false;
  bool is_plapps = false;

  // Shortcut activation from boot/capture hooks
  bool cheats_shortcut_activated = false;
  bool cheats_shortcut_activated_not_open = false;

  // Running big-app / cheats UI
  std::string running_tid;
  bool is_game_open = true;
  bool is_current_game_open = true;
  std::string current_menu_tid;
  std::string current_cheat_tid;
  int cheat_enabled_map[256]{};

  // Dynamic lists filled by generate_*_xml
  std::vector<Plugins> plugins_list;
  std::vector<Plugins> auto_list;
  std::vector<Payloads_Apps> payloads_apps_list;
  std::vector<GameEntry> games_list;

  std::string remote_play_info;

  // Overlay
  bool all_cpu_usage = false;

  std::atomic_bool cheat_action_in_progress{false};
  std::atomic_bool download_kstuff_thread_in_progress{false};
};

/** Single process-wide UI state for shellui.elf. */
extern ToolboxUiState g_ui;

/*
 * Compatibility references — same storage as g_ui members.
 * Prefer g_ui.field in new code.
 */
extern bool &is_plugin;
extern bool &is_su_menu;
extern bool &is_debug_settings;
extern bool &is_cheats;
extern bool &is_auto_plugin;
extern bool &is_remote_play;
extern bool &is_plapps;
extern bool &cheats_shortcut_activated;
extern bool &cheats_shortcut_activated_not_open;
extern bool &is_game_open;
extern bool &is_current_game_open;
extern bool &g_all_cpu_usage;

extern std::string &running_tid;
extern std::string &current_menu_tid;
extern std::string &currentCheatTID;
extern std::string &remote_play_info;

extern std::vector<GameEntry> &games_list;
extern std::vector<Plugins> &plugins_list;
extern std::vector<Plugins> &auto_list;
extern std::vector<Payloads_Apps> &payloads_apps_list;

/** Points at g_ui.cheat_enabled_map[0]. */
extern int *const cheatEnabledMap;

extern std::atomic_bool &cheat_action_in_progress;
extern std::atomic_bool &download_kstuff_thread_in_progress;
