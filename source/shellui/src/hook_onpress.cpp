/* Copyright (C) 2025 OrionHEN / LightningMods
 * ShellUI OnPress_Hook — table-driven toolbox press dispatch.
 */
#include "onpress.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

extern int (*oOnPress)(MonoObject *Instance, MonoObject *element, MonoObject *e);
extern bool is_current_game_open;

static OnPressResult try_exact(const OnPressExactEntry *table, size_t n,
                               OnPressContext &ctx) {
  for (size_t i = 0; i < n; ++i) {
    if (ctx.id == table[i].id) {
      return table[i].handler(ctx);
    }
  }
  return OnPressResult::NotMine;
}

static OnPressResult try_prefix(const OnPressPrefixEntry *table, size_t n,
                                OnPressContext &ctx) {
  for (size_t i = 0; i < n; ++i) {
    if (ctx.id.rfind(table[i].prefix, 0) == 0) {
      OnPressResult r = table[i].handler(ctx);
      if (r != OnPressResult::NotMine) {
        return r;
      }
    }
  }
  return OnPressResult::NotMine;
}

int OnPress_Hook(MonoObject *Instance, MonoObject *element, MonoObject *e) {
  static const std::vector<std::string> excludedIds = {
      "id_dl_cheats",
      "id_save_rp_info",
      "id_download_kstuff",
      "id_delete_kstuff",
  };

  if (!Instance || !element) {
#if SHELL_DEBUG == 1
    shellui_log("[LM HOOK] OnPress_Hook: args are null");
#endif
    return oOnPress(Instance, element, e);
  }

  OnPressContext ctx;
  ctx.instance = Instance;
  ctx.element = element;
  ctx.event = e;
  ctx.id = GetPropertyValue(element, "Id");
  ctx.value = GetPropertyValue(element, "Value");
  ctx.title = GetPropertyValue(element, "Title");

  bool is_cust_pkg = (ctx.id.rfind("id_pkg_", 0) == 0);
  bool is_orionhen_pl = (ctx.id.rfind("id_orionhen_pl_loader_", 0) == 0);

  if (ctx.id.rfind("id_cheat_", 0) == 0 && !is_current_game_open) {
    notify("The Game is not running, to activate cheats launch the game first");
#if SHELL_DEBUG == 1
    shellui_log("Failed to activate %s, game is not running", ctx.id.c_str());
#endif
    return oOnPress(Instance, element, e);
  }

  bool isExcludedId =
      std::find(excludedIds.begin(), excludedIds.end(), ctx.id) !=
      excludedIds.end();
  if (ctx.value.empty() && !isExcludedId && !is_cust_pkg && !is_orionhen_pl) {
#if SHELL_DEBUG == 1
    shellui_log("[LM HOOK] OnPress_Hook: Id: %s has no value set",
                ctx.id.c_str());
#endif
    return oOnPress(Instance, element, e);
  }

#if SHELL_DEBUG == 1
  shellui_log("[LM HOOK] OnPress_Hook: Id: %s, Value: %s", ctx.id.c_str(),
              ctx.value.c_str());
#endif

  OnPressResult result = OnPressResult::NotMine;
  size_t n = 0;

  // Prefix tables first (cheats/plugins/packages) so exact "id_plugin" list title
  // is not required — then exact tables.
  auto run_prefix = [&](const OnPressPrefixEntry *(*fn)(size_t *)) {
    if (result != OnPressResult::NotMine)
      return;
    const OnPressPrefixEntry *t = fn(&n);
    result = try_prefix(t, n, ctx);
  };
  auto run_exact = [&](const OnPressExactEntry *(*fn)(size_t *)) {
    if (result != OnPressResult::NotMine)
      return;
    const OnPressExactEntry *t = fn(&n);
    result = try_exact(t, n, ctx);
  };

  run_prefix(onpress_cheats_prefix);
  run_prefix(onpress_plugins_prefix);
  run_prefix(onpress_packages_prefix);
  run_exact(onpress_overlay_exact);
  run_exact(onpress_network_exact);
  run_exact(onpress_system_exact);
  run_exact(onpress_misc_exact);

  if (result == OnPressResult::EarlyReturn) {
    return oOnPress(Instance, element, e);
  }
  if (result == OnPressResult::NotMine) {
    shellui_log("Not a toolbox item!");
  }
  // Match legacy: persist after any non-early path (including unknown ids).
  if (ctx.dirty) {
    settings_commit(ctx.reload_main, ctx.reload_util);
  }

  return oOnPress(Instance, element, e);
}
