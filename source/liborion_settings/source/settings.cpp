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

#include <orion/settings.hpp>

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

// Header-only inih-style parser already used across the tree.
#include <ini.h>

namespace orion {
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
  out->DPI = atoi_def(ini_parser_get(parser, "Settings.DPI", "1"), 1) != 0;
  out->toolbox_auto_start =
      atoi_def(ini_parser_get(parser, "Settings.toolbox_auto_start", "1"), 1) != 0;
  out->disable_toolbox_auto_start_for_rest_mode =
      atoi_def(ini_parser_get(parser,
                              "Settings.disable_toolbox_auto_start_for_rest_mode",
                              "0"),
               0) != 0;
  out->util_rest_kill =
      atoi_def(ini_parser_get(parser, "Settings.Util_rest_kill", "0"), 0) != 0;
  out->game_rest_kill =
      atoi_def(ini_parser_get(parser, "Settings.Game_rest_kill", "0"), 0) != 0;
  out->start_option =
      atoi_def(ini_parser_get(parser, "Settings.StartOption", "0"), 0);
  out->rest_mode_delay_seconds = static_cast<uint64_t>(
      atol_def(ini_parser_get(parser, "Settings.Rest_Mode_Delay_Seconds", "0"), 0));
  out->libhijacker_cheats =
      atoi_def(ini_parser_get(parser, "Settings.libhijacker_cheats", "0"), 0) != 0;
  out->debug_app_jb_msg =
      atoi_def(ini_parser_get(parser, "Settings.APP_JB_Debug_Msg", "0"), 0) != 0;
  // Accept both legacy keys for the 9028 server.
  const char *legacy =
      ini_parser_get(parser, "Settings.legacy_cmd_server", nullptr);
  if (!legacy) {
    legacy = ini_parser_get(parser, "Settings.debug_legacy_cmd_server", "0");
  }
  out->legacy_cmd_server = atoi_def(legacy, 0) != 0;
  out->selected_cheats_repo =
      atoi_def(ini_parser_get(parser, "Settings.selected_cheats_repo", "0"), 0);
  out->auto_eject_disc =
      atoi_def(ini_parser_get(parser, "Settings.auto_eject_disc", "0"), 0) != 0;
  out->display_tids =
      atoi_def(ini_parser_get(parser, "Settings.Display_tids", "0"), 0) != 0;
  out->orionhen_game_opts =
      atoi_def(ini_parser_get(parser, "Settings.OrionHEN_Game_Options", "1"), 1) !=
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
  out->overlay_fps =
      atoi_def(ini_parser_get(parser, "Settings.overlay_fps", "0"), 0) != 0;
  out->overlay_ip =
      atoi_def(ini_parser_get(parser, "Settings.overlay_ip", "0"), 0) != 0;
  out->overlay_pos =
      atoi_def(ini_parser_get(parser, "Settings.Overlay_pos", "0"), 0);
  out->cheats_shortcut_opt =
      atoi_def(ini_parser_get(parser, "Settings.Cheats_shortcut_opt", "0"), 0);
  out->toolbox_shortcut_opt =
      atoi_def(ini_parser_get(parser, "Settings.Toolbox_shortcut_opt", "0"), 0);
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
  b += "DPI=" + std::to_string(in.DPI ? 1 : 0) + "\n";
  b += "toolbox_auto_start=" + std::to_string(in.toolbox_auto_start ? 1 : 0) +
       "\n";
  b += "disable_toolbox_auto_start_for_rest_mode=" +
       std::to_string(in.disable_toolbox_auto_start_for_rest_mode ? 1 : 0) + "\n";
  b += "Util_rest_kill=" + std::to_string(in.util_rest_kill ? 1 : 0) + "\n";
  b += "Game_rest_kill=" + std::to_string(in.game_rest_kill ? 1 : 0) + "\n";
  b += "StartOption=" + std::to_string(in.start_option) + "\n";
  b += "Rest_Mode_Delay_Seconds=" +
       std::to_string(static_cast<unsigned long long>(in.rest_mode_delay_seconds)) +
       "\n";
  b += "libhijacker_cheats=" + std::to_string(in.libhijacker_cheats ? 1 : 0) +
       "\n";
  b += "APP_JB_Debug_Msg=" + std::to_string(in.debug_app_jb_msg ? 1 : 0) + "\n";
  b += "legacy_cmd_server=" + std::to_string(in.legacy_cmd_server ? 1 : 0) + "\n";
  b += "selected_cheats_repo=" + std::to_string(in.selected_cheats_repo) + "\n";
  b += "auto_eject_disc=" + std::to_string(in.auto_eject_disc ? 1 : 0) + "\n";
  b += "Display_tids=" + std::to_string(in.display_tids ? 1 : 0) + "\n";
  b += "OrionHEN_Game_Options=" +
       std::to_string(in.orionhen_game_opts ? 1 : 0) + "\n";
  b += "enable_fan_speed=" + std::to_string(in.enable_fan_speed ? 1 : 0) + "\n";
  b += "fan_threshold=" + std::to_string(in.fan_threshold) + "\n";
  b += "overlay_ram=" + std::to_string(in.overlay_ram ? 1 : 0) + "\n";
  b += "overlay_cpu=" + std::to_string(in.overlay_cpu ? 1 : 0) + "\n";
  b += "overlay_gpu=" + std::to_string(in.overlay_gpu ? 1 : 0) + "\n";
  b += "overlay_fps=" + std::to_string(in.overlay_fps ? 1 : 0) + "\n";
  b += "overlay_ip=" + std::to_string(in.overlay_ip ? 1 : 0) + "\n";
  b += "Overlay_pos=" + std::to_string(in.overlay_pos) + "\n";
  b += "Cheats_shortcut_opt=" + std::to_string(in.cheats_shortcut_opt) + "\n";
  b += "Toolbox_shortcut_opt=" + std::to_string(in.toolbox_shortcut_opt) + "\n";
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
    if (settings_usb_disables_toolbox_auto_start()) {
      out->toolbox_auto_start = false;
    }
    return true;
  }
  if (try_load_path(kConfigPathShellui, out)) {
    if (settings_usb_disables_toolbox_auto_start()) {
      out->toolbox_auto_start = false;
    }
    return true;
  }
  if (settings_usb_disables_toolbox_auto_start()) {
    out->toolbox_auto_start = false;
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

bool settings_usb_disables_toolbox_auto_start() {
  struct stat st {};
  return stat("/mnt/usb0/toolbox_auto_start", &st) == 0;
}

} // namespace orion
