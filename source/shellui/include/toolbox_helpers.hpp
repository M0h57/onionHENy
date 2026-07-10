/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Pure ShellUI helpers (host-testable, no Mono/PS5 I/O).
 */
#pragma once

#include <cstring>
#include <cstddef>
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
 * Must end with ".plugin" / ".elf" and have a non-empty stem (rejects bare
 * ".elf" / ".plugin" which otherwise show as a nameless list entry).
 * Rejects .auto_start markers and "." / "..".
 */
inline bool is_plugin_or_elf_name(const char *name) {
  if (!name || !name[0])
    return false;
  if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0)
    return false;
  /* sidecar / marker files: foo.elf.auto_start */
  if (std::strstr(name, ".auto_start") != nullptr)
    return false;

  const std::size_t n = std::strlen(name);
  const bool is_elf = n > 4 && std::strcmp(name + (n - 4), ".elf") == 0;
  const bool is_plugin = n > 7 && std::strcmp(name + (n - 7), ".plugin") == 0;
  if (!is_elf && !is_plugin)
    return false;
  /* stem before extension must be non-empty → drop literal ".elf" / ".plugin" */
  return (is_elf ? n - 4 : n - 7) > 0;
}

/**
 * Launch/PID key for raw ELF: "foo.elf" → "foo". Matches util
 * orion_plugin_elf_key_from_name (must stay equivalent).
 */
inline bool elf_key_from_name(const char *name, char *out, std::size_t out_sz) {
  if (!name || !out || out_sz < 2)
    return false;
  const char *base = std::strrchr(name, '/');
  base = base ? base + 1 : name;
  if (!base[0] || std::strcmp(base, ".") == 0 || std::strcmp(base, "..") == 0)
    return false;
  std::size_t n = std::strlen(base);
  if (n >= 4 && std::strcmp(base + n - 4, ".elf") == 0)
    n -= 4;
  if (n == 0)
    return false;
  if (n >= out_sz)
    n = out_sz - 1;
  std::memcpy(out, base, n);
  out[n] = '\0';
  return out[0] != '\0';}

} // namespace toolbox
