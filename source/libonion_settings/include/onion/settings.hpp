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

// ---------------------------------------------------------------------------
// Single product config schema for daemon / util / shellui.
//
// Paths:
//   primary  /data/OnionHEN/config.ini          (elevated daemons)
//   shellui  /user/data/OnionHEN/config.ini     (SceShellUI sandbox view)
// Load tries both; save writes all writable targets so the files stay twins.
//
// Process-local store: use SettingsStore for multi-threaded daemons (snapshot
// under mutex). Multi-process copies of Settings are expected and OK.
//
// Force C++ linkage even when a parent includes us under extern "C"
// (e.g. util common_utils.h inside extern "C" blocks).
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C++" {

#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <utility>

namespace onion {

// Canonical filesystem paths (string keys match Settings.* INI keys).
inline constexpr const char *kConfigPathPrimary = "/data/OnionHEN/config.ini";
inline constexpr const char *kConfigPathShellui = "/user/data/OnionHEN/config.ini";

// Schema version written into INI as Settings.schema_version for future migrations.
inline constexpr int kSettingsSchemaVersion = 1;

struct Settings {
  // --- Rest mode / services ---
  bool util_rest_kill = false;
  bool game_rest_kill = false;
  uint64_t rest_mode_delay_seconds = 0;

  // --- Cheats / debug ---
  bool libhijacker_cheats = false;
  bool debug_app_jb_msg = false;
  bool legacy_cmd_server = false; // util 9028

  // --- Disc / UI ---
  bool display_tids = false;
  bool onionhen_game_opts = true;

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

  // --- UI language (shellui toolbox XML) ---
  // 0 = zh-Hans (default), 1 = English
  int ui_lang = 0;

  // Meta
  int schema_version = kSettingsSchemaVersion;
};

// Thread-safe process-local settings (daemon / util IPC + worker threads).
// ShellUI may keep a plain Settings if UI work is single-threaded; prefer this
// store when readers and writers can race.
class SettingsStore {
public:
  SettingsStore() = default;
  explicit SettingsStore(const Settings &s) : s_(s) {}

  Settings snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return s_;
  }

  void store(const Settings &s) {
    std::lock_guard<std::mutex> lock(mu_);
    s_ = s;
  }

  /** Mutate under lock; returns a copy of the new value. */
  template <typename Fn>
  Settings update(Fn &&fn) {
    std::lock_guard<std::mutex> lock(mu_);
    fn(s_);
    return s_;
  }

private:
  mutable std::mutex mu_;
  Settings s_{};
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

/**
 * Max st_mtime across both twin config paths (0 if neither exists).
 * Use for daemon reload gating so either path's write invalidates the cache.
 */
time_t settings_config_newest_mtime();

/**
 * True if any twin path has mtime strictly greater than `since`.
 * When no config file exists, returns false (nothing newer on disk).
 */
bool settings_config_is_newer_than(time_t since);

} // namespace onion

} // extern "C++"
#endif /* __cplusplus */
