/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Table-driven OnPress dispatch for ShellUI toolbox items.
 */

#pragma once

#include "HookedFuncs.hpp"
#include "ipc.hpp" // IPC_Client + shellui_log
#include <orion/platform.h>
#include <orion/proc_query.h>
#include <cstddef>
#include <string>

/** Per-press context shared by domain handlers. */
struct OnPressContext {
  MonoObject *instance = nullptr;
  MonoObject *element = nullptr;
  MonoObject *event = nullptr;
  std::string id;
  std::string value;
  std::string title;

  /** Request settings_commit after successful handling. */
  bool reload_main = false;
  bool reload_util = false;
  bool dirty = true; // SaveSettings by default when handled
};

/**
 * Handler result:
 * - Handled: domain handled the id; dispatcher will settings_commit (if dirty) then oOnPress.
 * - EarlyReturn: stop now; call oOnPress without commit (no-op / validation fail).
 * - NotMine: try next matcher / fall through.
 */
enum class OnPressResult {
  Handled,
  EarlyReturn,
  NotMine,
};

using OnPressHandler = OnPressResult (*)(OnPressContext &ctx);

struct OnPressExactEntry {
  const char *id;
  OnPressHandler handler;
};

struct OnPressPrefixEntry {
  const char *prefix;
  OnPressHandler handler;
};

/* Domain tables (defined in onpress_*.cpp). */
const OnPressExactEntry *onpress_overlay_exact(size_t *count);
const OnPressExactEntry *onpress_network_exact(size_t *count);
const OnPressExactEntry *onpress_system_exact(size_t *count);
const OnPressExactEntry *onpress_misc_exact(size_t *count);

const OnPressPrefixEntry *onpress_plugins_prefix(size_t *count);
const OnPressPrefixEntry *onpress_cheats_prefix(size_t *count);
const OnPressPrefixEntry *onpress_packages_prefix(size_t *count);

/** Shared toggle helpers. */
inline bool value_as_int(const OnPressContext &ctx) {
  return atoi(ctx.value.c_str());
}

inline bool value_as_long(const OnPressContext &ctx) {
  return atol(ctx.value.c_str());
}
