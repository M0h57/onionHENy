/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Extracted from MonoUtils.cpp for module locality.
 */

#include "HookedFuncs.hpp"
#include <orion/platform.h>
#include <orion/ready.h>
#include "ipc.hpp" // shellui_log + IPC_Client
#include <orion/settings.hpp>

void apply_overlay_layout() {
  if (g_settings.overlay_pos == OVERLAY_POS_TOP_LEFT) {
    g_overlay_layout.overlay_fps_x = 10.0f;
    g_overlay_layout.overlay_fps_y = 10.0f;
    g_overlay_layout.overlay_gpu_x = 10.0f;
    g_overlay_layout.overlay_gpu_y = 35.0f;
    g_overlay_layout.overlay_cpu_x = 10.0f;
    g_overlay_layout.overlay_cpu_y = 60.0f;
    g_overlay_layout.overlay_ram_x = 10.0f;
    g_overlay_layout.overlay_ram_y = 85.0f;
    g_overlay_layout.overlay_ip_x = 10.0f;
    g_overlay_layout.overlay_ip_y = 110.0f;
  } else if (g_settings.overlay_pos == OVERLAY_POS_BOTTOM_LEFT) {
    g_overlay_layout.overlay_ram_x = 10.0f;
    g_overlay_layout.overlay_ram_y = 970.0f;
    g_overlay_layout.overlay_cpu_x = 10.0f;
    g_overlay_layout.overlay_cpu_y = 990.0f;
    g_overlay_layout.overlay_gpu_x = 10.0f;
    g_overlay_layout.overlay_gpu_y = 1010.0f;
    g_overlay_layout.overlay_fps_x = 10.0f;
    g_overlay_layout.overlay_fps_y = 1030.0f;
    g_overlay_layout.overlay_ip_x = 10.0f;
    g_overlay_layout.overlay_ip_y = 1050.0f;
  } else if (g_settings.overlay_pos == OVERLAY_POS_TOP_RIGHT) {
    g_overlay_layout.overlay_fps_x = 1720.0f;
    g_overlay_layout.overlay_fps_y = 10.0f;
    g_overlay_layout.overlay_gpu_x = 1720.0f;
    g_overlay_layout.overlay_gpu_y = 35.0f;
    g_overlay_layout.overlay_cpu_x = 1720.0f;
    g_overlay_layout.overlay_cpu_y = 60.0f;
    g_overlay_layout.overlay_ram_x = 1720.0f;
    g_overlay_layout.overlay_ram_y = 85.0f;
    g_overlay_layout.overlay_ip_x = 1670.0f;
    g_overlay_layout.overlay_ip_y = 110.0f;
  } else if (g_settings.overlay_pos == OVERLAY_POS_BOTTOM_RIGHT) {
    g_overlay_layout.overlay_ram_x = 1720.0f;
    g_overlay_layout.overlay_ram_y = 970.0f;
    g_overlay_layout.overlay_cpu_x = 1720.0f;
    g_overlay_layout.overlay_cpu_y = 990.0f;
    g_overlay_layout.overlay_gpu_x = 1720.0f;
    g_overlay_layout.overlay_gpu_y = 1010.0f;
    g_overlay_layout.overlay_fps_x = 1720.0f;
    g_overlay_layout.overlay_fps_y = 1030.0f;
    g_overlay_layout.overlay_ip_x = 1670.0f;
    g_overlay_layout.overlay_ip_y = 1050.0f;
  }
}

bool LoadSettings()
{
  orion::Settings s{};
  /* false from settings_load means defaults only — not a hard failure. */
  if (!orion::settings_load(&s)) {
    shellui_log("config.ini missing; using defaults");
  } else {
    shellui_log("Loaded settings from %s", orion::settings_last_loaded_path());
  }

  // Process-local store (UI thread); twin disk paths via settings_load/save.
  g_settings = s;
  if (g_settings.overlay_fps) {
    orion_ready_signal(ORION_FLAG_FPS_OVERLAY);
  }
  apply_overlay_layout();
  /* Always true once defaults-or-file applied (prx boot requires success). */
  return true;
}

bool SaveSettings()
{
  if (!orion::settings_save(g_settings)) {
    shellui_log("Failed to save settings to any config path");
    return false;
  }
  shellui_log("Saved settings (primary + shellui paths when writable)");
  return true;
}

void settings_commit(bool reload_main, bool reload_util)
{
  SaveSettings();
  if (reload_main) {
    IPC_Client::getInstance(false).Reload_Daemon_Settings();
  }
  if (reload_util) {
    IPC_Client::getInstance(true).Reload_Daemon_Settings();
  }
}

