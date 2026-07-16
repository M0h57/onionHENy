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

#include <onion/settings.hpp>

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Header-only inih-style parser already used across the tree.
#include <ini.h>

namespace onion {
namespace {

const char *g_last_loaded = "";

bool path_exists(const char *path) {
  struct stat st {};
  return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool dir_writable_parent(const char *path) {
  // Best-effort: try open with create; callers still check write result.
  (void)path;
  return true;
}

int atoi_def(const char *s, int def) {
  return s ? atoi(s) : def;
}

long atol_def(const char *s, long def) {
  return s ? atol(s) : def;
}

void apply_parser(IniParser *parser, Settings *out) {
  out->util_rest_kill =
      atoi_def(ini_parser_get(parser, "Settings.Util_rest_kill", "0"), 0) != 0;
  out->game_rest_kill =
      atoi_def(ini_parser_get(parser, "Settings.Game_rest_kill", "0"), 0) != 0;
  out->rest_mode_delay_seconds = static_cast<uint64_t>(
      atol_def(ini_parser_get(parser, "Settings.Rest_Mode_Delay_Seconds", "0"), 0));
  out->libhijacker_cheats =
      atoi_def(ini_parser_get(parser, "Settings.libhijacker_cheats", "0"), 0) != 0;
  out->debug_app_jb_msg =
      atoi_def(ini_parser_get(parser, "Settings.APP_JB_Debug_Msg", "0"), 0) != 0;
  out->display_tids =
      atoi_def(ini_parser_get(parser, "Settings.Display_tids", "0"), 0) != 0;
  out->onionhen_game_opts =
      atoi_def(ini_parser_get(parser, "Settings.OnionHEN_Game_Options", "1"), 1) !=
      0;
  out->enable_fan_speed =
      atoi_def(ini_parser_get(parser, "Settings.enable_fan_speed", "0"), 0) != 0;
  out->fan_threshold =
      atoi_def(ini_parser_get(parser, "Settings.fan_threshold", "77"), 77);
  out->overlay_ram =
      atoi_def(ini_parser_get(parser, "Settings.overlay_ram", "1"), 1) != 0;
  out->overlay_cpu =
      atoi_def(ini_parser_get(parser, "Settings.overlay_cpu", "1"), 1) != 0;
  out->overlay_gpu =
      atoi_def(ini_parser_get(parser, "Settings.overlay_gpu", "1"), 1) != 0;
  out->overlay_ip =
      atoi_def(ini_parser_get(parser, "Settings.overlay_ip", "0"), 0) != 0;
  out->overlay_pos =
      atoi_def(ini_parser_get(parser, "Settings.Overlay_pos", "0"), 0);
  out->cheats_shortcut_opt =
      atoi_def(ini_parser_get(parser, "Settings.Cheats_shortcut_opt", "0"), 0);
  out->toolbox_shortcut_opt =
      atoi_def(ini_parser_get(parser, "Settings.Toolbox_shortcut_opt", "0"), 0);
  out->ui_lang = atoi_def(ini_parser_get(parser, "Settings.ui_lang", "0"), 0);
  if (out->ui_lang != 0 && out->ui_lang != 1)
    out->ui_lang = 0;
  out->schema_version =
      atoi_def(ini_parser_get(parser, "Settings.schema_version", "1"), 1);
}

bool write_path(const char *path, const std::string &body) {
  if (!path || !dir_writable_parent(path)) {
    return false;
  }
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (fd < 0) {
    return false;
  }
  ssize_t n = write(fd, body.data(), body.size());
  close(fd);
  return n == static_cast<ssize_t>(body.size());
}

bool try_load_path(const char *path, Settings *out) {
  if (!path_exists(path)) {
    return false;
  }
  IniParser parser{};
  if (!ini_parser_load(&parser, path)) {
    return false;
  }
  apply_parser(&parser, out);
  g_last_loaded = path;
  return true;
}

} // namespace

std::string settings_serialize(const Settings &in) {
  std::string b;
  b.reserve(1024);
  b += "[Settings]\n";
  b += "schema_version=" + std::to_string(kSettingsSchemaVersion) + "\n";
  b += "Util_rest_kill=" + std::to_string(in.util_rest_kill ? 1 : 0) + "\n";
  b += "Game_rest_kill=" + std::to_string(in.game_rest_kill ? 1 : 0) + "\n";
  b += "Rest_Mode_Delay_Seconds=" +
       std::to_string(static_cast<unsigned long long>(in.rest_mode_delay_seconds)) +
       "\n";
  b += "libhijacker_cheats=" + std::to_string(in.libhijacker_cheats ? 1 : 0) +
       "\n";
  b += "APP_JB_Debug_Msg=" + std::to_string(in.debug_app_jb_msg ? 1 : 0) + "\n";
  b += "Display_tids=" + std::to_string(in.display_tids ? 1 : 0) + "\n";
  b += "OnionHEN_Game_Options=" +
       std::to_string(in.onionhen_game_opts ? 1 : 0) + "\n";
  b += "enable_fan_speed=" + std::to_string(in.enable_fan_speed ? 1 : 0) + "\n";
  b += "fan_threshold=" + std::to_string(in.fan_threshold) + "\n";
  b += "overlay_ram=" + std::to_string(in.overlay_ram ? 1 : 0) + "\n";
  b += "overlay_cpu=" + std::to_string(in.overlay_cpu ? 1 : 0) + "\n";
  b += "overlay_gpu=" + std::to_string(in.overlay_gpu ? 1 : 0) + "\n";
  b += "overlay_ip=" + std::to_string(in.overlay_ip ? 1 : 0) + "\n";
  b += "Overlay_pos=" + std::to_string(in.overlay_pos) + "\n";
  b += "Cheats_shortcut_opt=" + std::to_string(in.cheats_shortcut_opt) + "\n";
  b += "Toolbox_shortcut_opt=" + std::to_string(in.toolbox_shortcut_opt) + "\n";
  b += "ui_lang=" + std::to_string(in.ui_lang) + "\n";
  return b;
}

bool settings_load_file(const char *path, Settings *out) {
  if (!out || !path) {
    return false;
  }
  *out = Settings{};
  return try_load_path(path, out);
}

bool settings_save_file(const char *path, const Settings &in) {
  if (!path) {
    return false;
  }
  return write_path(path, settings_serialize(in));
}

bool settings_load(Settings *out) {
  if (!out) {
    return false;
  }
  *out = Settings{}; // defaults
  g_last_loaded = "";

  // Prefer elevated path, then shellui sandbox view.
  if (try_load_path(kConfigPathPrimary, out)) {
    return true;
  }
  if (try_load_path(kConfigPathShellui, out)) {
    return true;
  }
  return false;
}

bool settings_save(const Settings &in) {
  const std::string body = settings_serialize(in);
  bool ok = false;
  if (write_path(kConfigPathPrimary, body)) {
    ok = true;
  }
  if (write_path(kConfigPathShellui, body)) {
    ok = true;
  }
  return ok;
}

bool settings_ensure_default() {
  if (path_exists(kConfigPathPrimary) || path_exists(kConfigPathShellui)) {
    return true;
  }
  Settings def{};
  return settings_save(def);
}

const char *settings_last_loaded_path() { return g_last_loaded; }

static time_t path_mtime(const char *path) {
  if (!path) {
    return 0;
  }
  struct stat st {};
  if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
    return 0;
  }
  return st.st_mtime;
}

time_t settings_config_newest_mtime() {
  const time_t a = path_mtime(kConfigPathPrimary);
  const time_t b = path_mtime(kConfigPathShellui);
  return a > b ? a : b;
}

bool settings_config_is_newer_than(time_t since) {
  const time_t newest = settings_config_newest_mtime();
  if (newest == 0) {
    return false;
  }
  return newest > since;
}

} // namespace onion
