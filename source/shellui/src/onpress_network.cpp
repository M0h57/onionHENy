/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress network domain */
#include "onpress.hpp"
#include <cstdlib>

static OnPressResult id_debug_legacy_cmd(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.legacy_cmd_server) {
    shellui_log("Debug cmd already %s",
                g_settings.legacy_cmd_server ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  g_settings.legacy_cmd_server = !g_settings.legacy_cmd_server;
  if (IPC_Client::getInstance(true).ToggleSetting(
          BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER, g_settings.legacy_cmd_server) !=
      IPC_Ret::NO_ERROR) {
    notify(g_settings.legacy_cmd_server ? "cmd Failed to Start ..."
                                        : "CMD Server Failed to Stop ...");
    g_settings.legacy_cmd_server = !g_settings.legacy_cmd_server;
  }
  return OnPressResult::Handled;
}

static OnPressResult id_disp_titleids(OnPressContext &ctx) {
  bool &dis_tids = g_settings.display_tids;
  if (atol(ctx.value.c_str()) == dis_tids) {
    shellui_log("Display TIDs already %s", dis_tids ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  dis_tids = !dis_tids;
  ReloadRNPSApp("NPXS40002");
  return OnPressResult::Handled;
}

static const OnPressExactEntry kExact[] = {
    {"id_debug_legacy_cmd", id_debug_legacy_cmd},
    {"id_disp_titleids", id_disp_titleids},
};

const OnPressExactEntry *onpress_network_exact(size_t *count) {
  *count = sizeof(kExact) / sizeof(kExact[0]);
  return kExact;
}
