/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress overlay domain */
#include "onpress.hpp"
#include <orion/ready.h>
#include <orion/fps_shm.h>
#include <cstdlib>
#include <unistd.h>

void RemoveGameWidget(RemoveWidget widget);
void CreateGameWidget(CreateWidget widget);

/** Tear down all segments, recompute horizontal packing, rebuild enabled ones. */
static void rebuild_overlay_bar() {
  RemoveGameWidget(REMOVE_ALL_OVERLAYS);
  apply_overlay_layout();
  if (g_settings.overlay_fps)
    CreateGameWidget(CREATE_FPS_OVERLAY);
  if (g_settings.overlay_cpu || g_ui.all_cpu_usage)
    CreateGameWidget(CREATE_CPU_OVERLAY);
  if (g_settings.overlay_gpu)
    CreateGameWidget(CREATE_GPU_OVERLAY);
  if (g_settings.overlay_ram)
    CreateGameWidget(CREATE_RAM_OVERLAY);
  if (g_settings.overlay_ip)
    CreateGameWidget(CREATE_IP_OVERLAY);
}

static OnPressResult toggle_overlay_flag(OnPressContext &ctx, bool &flag,
                                         bool fps_special = false) {
  if (atoi(ctx.value.c_str()) == flag) {
    return OnPressResult::EarlyReturn;
  }
  flag = !flag;
  if (fps_special) {
    if (flag)
      orion_ready_signal(ORION_FLAG_FPS_OVERLAY);
    else
      orion_ready_clear(ORION_FLAG_FPS_OVERLAY);
  }
  rebuild_overlay_bar();
  return OnPressResult::Handled;
}

static OnPressResult id_overlay_gpu(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_gpu);
}

static OnPressResult id_overlay_cpu(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.overlay_cpu) {
    return OnPressResult::EarlyReturn;
  }
  if (!atoi(ctx.value.c_str()) && g_ui.all_cpu_usage) {
    notify("To disable CPU overlay, please disable the All CPU usage option first");
    return OnPressResult::EarlyReturn;
  }
  g_settings.overlay_cpu = !g_settings.overlay_cpu;
  rebuild_overlay_bar();
  return OnPressResult::Handled;
}

static OnPressResult id_overlay_ram(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_ram);
}

static OnPressResult id_overlay_fps(OnPressContext &ctx) {
  OnPressResult r = toggle_overlay_flag(ctx, g_settings.overlay_fps, true);
  /* Privileged: pre-create SHM so the next game inject can open it. */
  if (g_settings.overlay_fps)
    (void)orion_fps_shm_ensure();
  return r;
}

static OnPressResult id_overlay_ip(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_ip);
}

static OnPressResult id_all_cpu_usage(OnPressContext &ctx) {
  if (g_ui.all_cpu_usage == atoi(ctx.value.c_str())) {
    return OnPressResult::EarlyReturn;
  }
  if (!g_settings.overlay_cpu) {
    notify("To change CPU overlay mode, please enable the CPU overlay first");
    return OnPressResult::EarlyReturn;
  }
  g_ui.all_cpu_usage = !g_ui.all_cpu_usage;
  rebuild_overlay_bar();
  return OnPressResult::Handled;
}

static OnPressResult id_overlay_change_pos(OnPressContext &ctx) {
  if ((overlay_positions)atoi(ctx.value.c_str()) == g_settings.overlay_pos) {
    return OnPressResult::EarlyReturn;
  }
  g_settings.overlay_pos = atoi(ctx.value.c_str());
  rebuild_overlay_bar();
  return OnPressResult::Handled;
}

static const OnPressExactEntry kExact[] = {
    {"id_overlay_gpu", id_overlay_gpu},
    {"id_overlay_cpu", id_overlay_cpu},
    {"id_overlay_ram", id_overlay_ram},
    {"id_overlay_fps", id_overlay_fps},
    {"id_overlay_ip", id_overlay_ip},
    {"id_all_cpu_usage", id_all_cpu_usage},
    {"id_overlay_change_pos", id_overlay_change_pos},
};

const OnPressExactEntry *onpress_overlay_exact(size_t *count) {
  *count = sizeof(kExact) / sizeof(kExact[0]);
  return kExact;
}
