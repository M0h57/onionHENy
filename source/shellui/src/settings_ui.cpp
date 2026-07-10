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
  /*
   * PHU flex banner (phu_overlay.elf):
   *   Panel X=0, Width = RootWidget full width ("Width=fullscreen"),
   *   Y = 0 (top edge) or screenH - barH (bottom edge),
   *   Height = font_size + 6.
   * Metrics pack L→R and are centered *inside* the full-width strip.
   */
  constexpr float kScreenW = 1920.0f;
  constexpr float kScreenH = 1080.0f;
  /* PHU: font_size=18, panel height = font + 6. */
  constexpr float kFontH = 18.0f;
  constexpr float kBarExtra = 6.0f;
  constexpr float w_fps = 120.0f;
  constexpr float w_cpu_avg = 190.0f;
  constexpr float w_cpu_all = 420.0f;
  constexpr float w_gpu = 190.0f;
  constexpr float w_ram = 170.0f;
  constexpr float w_ip = 200.0f;
  constexpr float kGap = 28.0f; /* roomy group gap (not packed) */
  constexpr float kOffscreen = -4096.0f;

  const bool show_fps = g_settings.overlay_fps;
  const bool show_cpu = g_settings.overlay_cpu || g_ui.all_cpu_usage;
  const bool show_gpu = g_settings.overlay_gpu;
  const bool show_ram = g_settings.overlay_ram;
  const bool show_ip = g_settings.overlay_ip;
  const float w_cpu = g_ui.all_cpu_usage ? w_cpu_all : w_cpu_avg;

  float content_w = 0.0f;
  int n = 0;
  auto acc = [&](bool on, float w) {
    if (!on)
      return;
    if (n++)
      content_w += kGap;
    content_w += w;
  };
  acc(show_fps, w_fps);
  acc(show_cpu, w_cpu);
  acc(show_gpu, w_gpu);
  acc(show_ram, w_ram);
  acc(show_ip, w_ip);

  const bool bottom =
      (g_settings.overlay_pos == OVERLAY_POS_BOTTOM_LEFT ||
       g_settings.overlay_pos == OVERLAY_POS_BOTTOM_RIGHT);

  const float bar_h = kFontH + kBarExtra;
  /* Full-bleed edge strip — PHU: X=0, Width=root width. */
  const float bar_x = 0.0f;
  const float bar_w = kScreenW;
  const float bar_y = bottom ? (kScreenH - bar_h) : 0.0f;
  /*
   * Label cells use Y = bar_y and Height = bar_h with VerticalAlignment=Center
   * so text is vertically middle of the strip (see CreateGameWidget / set_label_xy).
   */
  float x = content_w > 0.0f ? ((kScreenW - content_w) * 0.5f) : kOffscreen;

  g_overlay_layout.bar_x = bar_x;
  g_overlay_layout.bar_y = bar_y;
  g_overlay_layout.bar_w = bar_w;
  g_overlay_layout.bar_h = bar_h;

  auto place = [&](float &ox, float &oy, bool on, float w) {
    oy = bar_y;
    if (!on) {
      ox = kOffscreen;
      return;
    }
    ox = x;
    x += w + kGap;
  };

  place(g_overlay_layout.overlay_fps_x, g_overlay_layout.overlay_fps_y,
        show_fps, w_fps);
  place(g_overlay_layout.overlay_cpu_x, g_overlay_layout.overlay_cpu_y,
        show_cpu, w_cpu);
  place(g_overlay_layout.overlay_gpu_x, g_overlay_layout.overlay_gpu_y,
        show_gpu, w_gpu);
  place(g_overlay_layout.overlay_ram_x, g_overlay_layout.overlay_ram_y,
        show_ram, w_ram);
  place(g_overlay_layout.overlay_ip_x, g_overlay_layout.overlay_ip_y, show_ip,
        w_ip);
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

