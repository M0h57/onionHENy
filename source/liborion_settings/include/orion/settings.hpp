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

#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Single product config schema for daemon / util / shellui.
//
// Paths:
//   primary  /data/OrionHEN/config.ini          (elevated daemons)
//   shellui  /user/data/OrionHEN/config.ini     (SceShellUI sandbox view)
// Load tries both; save writes all writable targets so the files stay twins.
// ---------------------------------------------------------------------------

// Force C++ linkage even if a parent header included us under extern "C".
#ifdef __cplusplus
extern "C++" {
#endif

namespace orion {

// Canonical filesystem paths (string keys match Settings.* INI keys).
inline constexpr const char *kConfigPathPrimary = "/data/OrionHEN/config.ini";
inline constexpr const char *kConfigPathShellui = "/user/data/OrionHEN/config.ini";

// Schema version written into INI as Settings.schema_version for future migrations.
inline constexpr int kSettingsSchemaVersion = 1;

struct Settings {
  // --- Network / install ---
  bool DPI = true;
  bool DPI_v2 = false;

  // --- Toolbox ---
  bool toolbox_auto_start = true;
  bool disable_toolbox_auto_start_for_rest_mode = false;
  bool util_rest_kill = false;
  bool game_rest_kill = false;
  int start_option = 0; // 0=NONE, 1=HOME_MENU, 2=SETTINGS, 3=TOOLBOX
  uint64_t rest_mode_delay_seconds = 0;

  // --- Cheats / debug ---
  bool libhijacker_cheats = false;
  bool debug_app_jb_msg = false;
  bool legacy_cmd_server = false; // util 9028
  int selected_cheats_repo = 0;   // 0=OrionHEN, 1=GoldHEN

  // --- Disc / UI ---
  bool auto_eject_disc = false;
  bool display_tids = false;
  bool orionhen_game_opts = true;

  // --- Fan ---
  bool enable_fan_speed = false;
  int fan_threshold = 77;

  // --- Overlay (fps_elf / shellui) ---
  bool overlay_ram = true;
  bool overlay_cpu = true;
  bool overlay_gpu = true;
  bool overlay_fps = false;
  bool overlay_ip = false;
  int overlay_pos = 0; // 0 TL, 1 TR, 2 BL, 3 BR

  // --- Shortcuts (shellui) ---
  int cheats_shortcut_opt = 0;
  int toolbox_shortcut_opt = 0;

  // Meta
  int schema_version = kSettingsSchemaVersion;
};

// Fill `out` with defaults then overlay values from the first readable config path.
// Returns true if a file was loaded; false means defaults only (file missing).
bool settings_load(Settings *out);

// Load/save a single explicit path (host tests and tooling).
bool settings_load_file(const char *path, Settings *out);
bool settings_save_file(const char *path, const Settings &in);

// Serialize full schema to INI text (no I/O).
std::string settings_serialize(const Settings &in);

// Serialize full schema. Writes every path that can be opened for write.
// Returns true if at least one write succeeded.
bool settings_save(const Settings &in);

// Create default full-schema INI if no config exists on any path.
// Returns true if a file was created (or already existed after load attempt).
bool settings_ensure_default();

// Path that was last successfully loaded (empty if defaults only).
const char *settings_last_loaded_path();

// USB override used historically by daemons.
bool settings_usb_disables_toolbox_auto_start();

} // namespace orion

#ifdef __cplusplus
} // extern "C++"
#endif
