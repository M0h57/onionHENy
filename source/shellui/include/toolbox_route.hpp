/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Pure resource-name → toolbox page routing (host-testable, no Mono/PS5).
 * Used by GetManifestResourceStream_Hook.
 */
#pragma once

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace toolbox {

/** Which dynamic (or special) page to serve for a Legacy settings resource. */
enum class Page : unsigned char {
  None = 0,           /**< unknown → original stream */
  DebugSettings,      /**< embedded toolbox XML */
  Plugins,
  Cheats,
  AutoPlugins,
  RemotePlay,
  Plapps,
  SuperuserPass,      /**< recognized; still use original stream */
  RedirectOgDebug,    /**< og_debug.xml → debug_settings resource */
};

struct ResourceNames {
  std::string_view plugin_xml;
  std::string_view debug_settings_xml;
  std::string_view cheats_xml;
  std::string_view remote_play_xml;
};

struct RouteInput {
  std::string_view resource;
  ResourceNames names;
  bool cheats_shortcut = false;
  bool cheats_shortcut_not_open = false;
};

/** Flag snapshot after matching resource (matches historical hook behaviour). */
struct RouteFlags {
  bool is_plugin = false;
  bool is_su_menu = false;
  bool is_debug_settings = false;
  bool is_cheats = false;
  bool is_auto_plugin = false;
  bool is_remote_play = false;
  bool is_plapps = false;
};

struct RouteResult {
  Page page = Page::None;
  RouteFlags flags{};
  /** True when cheats page is selected via shortcut override. */
  bool shortcut_forced_cheats = false;
  /**
   * When serving cheats, caller should clear both shortcut flags after
   * generating XML (historical behaviour).
   */
  bool clear_cheat_shortcuts_after = false;
};

/**
 * Resolve which toolbox page to serve for @p in.resource.
 * Pure: no I/O, no globals.
 */
RouteResult resolve_resource(const RouteInput &in);

/** Fixed Legacy resource suffixes used by ShellUI (not base64-decoded). */
inline constexpr std::string_view kAutoPluginsXml =
    "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.auto_plugins.xml";
inline constexpr std::string_view kPlappsXml =
    "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.plapps.xml";
inline constexpr std::string_view kSuperuserXml =
    "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.superuser.xml";
inline constexpr std::string_view kOgDebugXml =
    "Sce.Vsh.ShellUI.Legacy.src.Sce.Vsh.ShellUI.Settings.Plugins.og_debug.xml";

// ---- Cheat map helpers (host-testable) ----

constexpr std::size_t kCheatMapSize = 256;

/**
 * If @p new_tid differs from @p current_tid, clear map and update current_tid.
 * Returns true if the map was reset.
 */
inline bool reset_cheat_map_if_tid_changed(std::string &current_tid, int *map,
                                           std::size_t map_n,
                                           std::string_view new_tid) {
  if (!map || map_n == 0)
    return false;
  if (current_tid == new_tid)
    return false;
  current_tid.assign(new_tid);
  std::memset(map, 0, map_n * sizeof(int));
  return true;
}

inline void set_cheat_enabled(int *map, std::size_t map_n, int cheat_id,
                              bool enabled) {
  if (!map || cheat_id < 0 || static_cast<std::size_t>(cheat_id) >= map_n)
    return;
  map[cheat_id] = enabled ? 1 : 0;
}

inline bool get_cheat_enabled(const int *map, std::size_t map_n, int cheat_id) {
  if (!map || cheat_id < 0 || static_cast<std::size_t>(cheat_id) >= map_n)
    return false;
  return map[cheat_id] != 0;
}

} // namespace toolbox
