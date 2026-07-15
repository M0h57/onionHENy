/* Copyright (C) 2025 OnionHEN / LightningMods — OnPress system settings domain */
#include "onpress.hpp"
#include <cstdlib>

static OnPressResult id_debug_jb(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.debug_app_jb_msg) {
    shellui_log("Debug JB already %s",
                g_settings.debug_app_jb_msg ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  g_settings.debug_app_jb_msg = !g_settings.debug_app_jb_msg;
  ctx.reload_main = true;
  return OnPressResult::Handled;
}

static OnPressResult id_custom_game_opts(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.onionhen_game_opts) {
    shellui_log("OnionHEN Game Options already %s",
                g_settings.onionhen_game_opts ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  g_settings.onionhen_game_opts = !g_settings.onionhen_game_opts;
  shellui_log("OnionHEN Game Options: %s",
              g_settings.onionhen_game_opts ? "Enabled" : "Disabled");
  return OnPressResult::Handled;
}

static OnPressResult id_selected_cheats_repo(OnPressContext &ctx) {
  g_settings.selected_cheats_repo = atoi(ctx.value.c_str());
  shellui_log("Selected cheats repo: %s",
              g_settings.selected_cheats_repo == CHEATS_REPO_ONIONHEN
                  ? "OnionHEN PS5"
                  : "GoldHEN PS4");
  return OnPressResult::Handled;
}

static OnPressResult id_ui_lang(OnPressContext &ctx) {
  int v = atoi(ctx.value.c_str());
  if (v != 0 && v != 1)
    v = 0;
  if (v == g_settings.ui_lang)
    return OnPressResult::EarlyReturn;
  g_settings.ui_lang = v;
  shellui_log("UI language: %s", v == 1 ? "en" : "zh-Hans");
  /* XML is built when the page opens; current tree stays in the old language. */
  if (v == 1) {
    notify("Language saved. Leave and re-open the toolbox for it to take effect.");
  } else {
    notify("语言已保存。退出并重新打开工具箱后生效。");
  }
  return OnPressResult::Handled;
}

static OnPressResult id_rest_1(OnPressContext &ctx) {
  g_settings.rest_mode_delay_seconds = atol(ctx.value.c_str());
  return OnPressResult::Handled;
}

static OnPressResult id_rest_2(OnPressContext &ctx) {
  bool &util_rest_kill = g_settings.util_rest_kill;
  if (atoi(ctx.value.c_str()) == util_rest_kill) {
    shellui_log("util_rest_kill already %s",
                util_rest_kill ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  util_rest_kill = !util_rest_kill;
  return OnPressResult::Handled;
}

static OnPressResult id_rest_3(OnPressContext &ctx) {
  bool &game_rest_kill = g_settings.game_rest_kill;
  if (atoi(ctx.value.c_str()) == game_rest_kill) {
    shellui_log("game_rest_kill already %s",
                game_rest_kill ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  game_rest_kill = !game_rest_kill;
  return OnPressResult::Handled;
}

static OnPressResult id_enable_fan_speed(OnPressContext &ctx) {
  if (atol(ctx.value.c_str()) == g_settings.enable_fan_speed) {
    shellui_log("Fan speed control already %s",
                g_settings.enable_fan_speed ? "Enabled" : "Disabled");
    return OnPressResult::EarlyReturn;
  }
  g_settings.enable_fan_speed = !g_settings.enable_fan_speed;
  IPC_Client::getInstance(false).Set_Fan_Threshold(g_settings.fan_threshold,
                                                   g_settings.enable_fan_speed);
  return OnPressResult::Handled;
}

static OnPressResult id_fan_speed(OnPressContext &ctx) {
  int &fan_speed = g_settings.fan_threshold;
  fan_speed = atoi(ctx.value.c_str());
  if (!g_settings.enable_fan_speed) {
    notify("Manual Fan speed threshold is not enabled");
    return OnPressResult::EarlyReturn;
  }
  shellui_log("Setting fan speed to %d%%", fan_speed);
  IPC_Client::getInstance(false).Set_Fan_Threshold(fan_speed,
                                                   g_settings.enable_fan_speed);
  return OnPressResult::Handled;
}

static OnPressResult id_auto_eject(OnPressContext &ctx) {
  g_settings.auto_eject_disc = atol(ctx.value.c_str());
  return OnPressResult::Handled;
}

static OnPressResult id_cheats_shortcut(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.cheats_shortcut_opt) {
    shellui_log("Cheats_shortcut already %i", g_settings.cheats_shortcut_opt);
    return OnPressResult::EarlyReturn;
  }
  Cheats_Shortcut opt = (Cheats_Shortcut)atoi(ctx.value.c_str());
  if (opt == CHEATS_SINGLE_SHARE) {
    if (g_settings.toolbox_shortcut_opt == TOOLBOX_SINGLE_SHARE) {
      notify("Toolbox and Cheats shortcuts cannot be the same, current "
             "selection will NOT be saved");
      return OnPressResult::EarlyReturn;
    }
  } else if (opt == CHEATS_LONG_SHARE) {
    if (g_settings.toolbox_shortcut_opt == TOOLBOX_LONG_SHARE) {
      notify("Toolbox and Cheats long shortcuts cannot be the same, current "
             "selection will NOT be saved");
      return OnPressResult::EarlyReturn;
    }
  }
  g_settings.cheats_shortcut_opt = static_cast<int>(opt);
  return OnPressResult::Handled;
}

static OnPressResult id_toolbox_shortcut(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.toolbox_shortcut_opt) {
    shellui_log("toolbox_shortcut_opt already %i",
                g_settings.toolbox_shortcut_opt);
    return OnPressResult::EarlyReturn;
  }
  Toolbox_Shortcut opt = (Toolbox_Shortcut)atoi(ctx.value.c_str());
  if (opt == TOOLBOX_SINGLE_SHARE) {
    if (g_settings.cheats_shortcut_opt == CHEATS_SINGLE_SHARE) {
      notify("Cheats and Toolbox shortcuts cannot be the same, current "
             "selection will NOT be saved");
      return OnPressResult::EarlyReturn;
    }
  } else if (opt == TOOLBOX_LONG_SHARE) {
    if (g_settings.cheats_shortcut_opt == CHEATS_LONG_SHARE) {
      notify("Cheats and Toolbox long shortcuts cannot be the same, current "
             "selection will NOT be saved");
      return OnPressResult::EarlyReturn;
    }
  }
  g_settings.toolbox_shortcut_opt = static_cast<int>(opt);
  return OnPressResult::Handled;
}

static const OnPressExactEntry kExact[] = {
    {"id_debug_jb", id_debug_jb},
    {"id_custom_game_opts", id_custom_game_opts},
    {"id_selected_cheats_repo", id_selected_cheats_repo},
    {"id_ui_lang", id_ui_lang},
    {"id_rest_1", id_rest_1},
    {"id_rest_2", id_rest_2},
    {"id_rest_3", id_rest_3},
    {"id_enable_fan_speed", id_enable_fan_speed},
    {"id_fan_speed", id_fan_speed},
    {"id_auto_eject", id_auto_eject},
    {"id_cheats_shortcut", id_cheats_shortcut},
    {"id_toolbox_shortcut", id_toolbox_shortcut},
};

const OnPressExactEntry *onpress_system_exact(size_t *count) {
  *count = sizeof(kExact) / sizeof(kExact[0]);
  return kExact;
}
