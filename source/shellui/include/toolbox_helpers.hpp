/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Pure ShellUI helpers (host-testable, no Mono/PS5 I/O).
 */
#pragma once

#include <cstring>
#include <string>

namespace toolbox {

/**
 * UI-facing path: strip "/user" mount prefix, map "/usb*" → "/mnt/usb*".
 * Only exact "/user" or paths under "/user/…" (not "/userdata").
 */
inline std::string display_path_for_ui(const std::string &path) {
  if (path == "/user")
    return {};
  if (path.rfind("/user/", 0) == 0)
    return path.substr(5); /* keep leading '/' of remainder */
  if (path.rfind("/usb", 0) == 0)
    return "/mnt" + path;
  return path;
}

/**
 * True for plugin/payload basenames eligible for the plugins list.
 * Accepts .plugin / .elf; rejects .auto_start markers.
 */
inline bool is_plugin_or_elf_name(const char *name) {
  if (!name || !name[0])
    return false;
  const bool is_elf = std::strstr(name, ".elf") != nullptr;
  const bool is_plugin_ext = std::strstr(name, ".plugin") != nullptr;
  const bool is_auto = std::strstr(name, ".auto_start") != nullptr;
  return (is_plugin_ext || is_elf) && !is_auto;
}

} // namespace toolbox
