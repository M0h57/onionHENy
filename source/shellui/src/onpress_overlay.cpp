/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress overlay domain */
#include "onpress.hpp"
#include <cstdlib>
#include <unistd.h>

void RemoveGameWidget(RemoveWidget widget);
void CreateGameWidget(CreateWidget widget);

static OnPressResult toggle_overlay_flag(OnPressContext &ctx, bool &flag,
                                         RemoveWidget rem, CreateWidget cre,
                                         bool fps_special = false) {
  if (atoi(ctx.value.c_str()) == flag) {
    return OnPressResult::EarlyReturn;
  }
  if (!atoi(ctx.value.c_str())) {
    if (fps_special) {
      RemoveGameWidget(rem);
      unlink("/system_tmp/fps_enabled");
    } else {
      RemoveGameWidget(rem);
    }
  } else {
    CreateGameWidget(cre);
    if (fps_special) {
      touch_file("/system_tmp/fps_enabled");
    }
  }
  flag = !flag;
  return OnPressResult::Handled;
}

static OnPressResult id_overlay_gpu(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_gpu, REMOVE_GPU_OVERLAY,
                             CREATE_GPU_OVERLAY);
}

static OnPressResult id_overlay_cpu(OnPressContext &ctx) {
  if (atoi(ctx.value.c_str()) == g_settings.overlay_cpu) {
    return OnPressResult::EarlyReturn;
  }
  if (!atoi(ctx.value.c_str())) {
    if (!g_all_cpu_usage) {
      RemoveGameWidget(REMOVE_CPU_OVERLAY);
    } else {
      notify("To disable CPU overlay, please disable the All CPU usage option first");
      return OnPressResult::EarlyReturn;
    }
  } else {
    CreateGameWidget(CREATE_CPU_OVERLAY);
  }
  g_settings.overlay_cpu = !g_settings.overlay_cpu;
  return OnPressResult::Handled;
}

static OnPressResult id_overlay_ram(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_ram, REMOVE_RAM_OVERLAY,
                             CREATE_RAM_OVERLAY);
}

static OnPressResult id_overlay_fps(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_fps, REMOVE_FPS_OVERLAY,
                             CREATE_FPS_OVERLAY, true);
}

static OnPressResult id_overlay_ip(OnPressContext &ctx) {
  return toggle_overlay_flag(ctx, g_settings.overlay_ip, REMOVE_IP_OVERLAY,
                             CREATE_IP_OVERLAY);
}

static OnPressResult id_all_cpu_usage(OnPressContext &ctx) {
  if (g_all_cpu_usage == atoi(ctx.value.c_str())) {
    return OnPressResult::EarlyReturn;
  }
  if (!g_settings.overlay_cpu) {
    notify("To change CPU overlay mode, please enable the CPU overlay first");
    return OnPressResult::EarlyReturn;
  }
  g_all_cpu_usage = !g_all_cpu_usage;
  return OnPressResult::Handled;
}

static OnPressResult id_overlay_change_pos(OnPressContext &ctx) {
  if ((overlay_positions)atoi(ctx.value.c_str()) == g_settings.overlay_pos) {
    return OnPressResult::EarlyReturn;
  }
  g_settings.overlay_pos = atoi(ctx.value.c_str());
  apply_overlay_layout();

  if (g_settings.overlay_cpu) {
    RemoveGameWidget(REMOVE_CPU_OVERLAY);
    CreateGameWidget(CREATE_CPU_OVERLAY);
  }
  if (g_settings.overlay_ram) {
    RemoveGameWidget(REMOVE_RAM_OVERLAY);
    CreateGameWidget(CREATE_RAM_OVERLAY);
  }
  if (g_settings.overlay_gpu) {
    RemoveGameWidget(REMOVE_GPU_OVERLAY);
    CreateGameWidget(CREATE_GPU_OVERLAY);
  }
  if (g_settings.overlay_fps) {
    RemoveGameWidget(REMOVE_FPS_OVERLAY);
    CreateGameWidget(CREATE_FPS_OVERLAY);
  }
  if (g_settings.overlay_ip) {
    RemoveGameWidget(REMOVE_IP_OVERLAY);
    CreateGameWidget(CREATE_IP_OVERLAY);
  }
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
