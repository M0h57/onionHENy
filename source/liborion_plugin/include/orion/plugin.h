/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Shared .plugin / .elf load helpers (PID files + elfldr :9021).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** On-disk / in-file package header (must stay binary-compatible). */
typedef struct OrionPluginHeader {
  char prefix[15];        /* "OrionHEN_PLUGIN" + NUL */
  char titleID[10];       /* 4 letters + 5 digits + NUL */
  char plugin_version[5]; /* "xx.xx" style */
} OrionPluginHeader;

/* Historical name used by util / shellui. */
typedef OrionPluginHeader CustomPluginHeader;

bool orion_plugin_is_elf(const void *buf, size_t size);
bool orion_plugin_is_valid(const void *buf);
/** ELF bytes start immediately after the package header. */
const uint8_t *orion_plugin_package_elf(const void *buf);

void orion_plugin_pid_path(char *out, size_t out_sz, const char *title_id);
pid_t orion_plugin_read_pid_file(const char *pid_path);
void orion_plugin_write_pid_file(const char *pid_path, pid_t pid);

/** Kill process recorded in /system_tmp/<titleID>.PID if still valid. */
void orion_plugin_stop_by_title(const char *title_id);

/**
 * Write ELF to /data/OrionHEN/plugins/<titleID>.elf and launch via 9021.
 * On success returns pid (>=0). If process name is not observed, returns 1.
 * On failure returns -1.
 */
pid_t orion_plugin_launch_9021(const char *title_id, const uint8_t *elf,
                               size_t elf_sz);

/** malloc'd file contents; caller free(). NULL on error. */
uint8_t *orion_plugin_read_file(const char *path, size_t *out_size);

/**
 * Optional hooks for load behaviour differences (util vs bootstrapper).
 * Pass NULL for defaults: no package prep, no EORR delete, require launch ok.
 */
typedef struct orion_plugin_load_opts {
  /**
   * Invoked for valid .plugin packages after stop, before 9021 launch
   * (util: make_plugin_app). May be NULL.
   */
  void (*prepare_package)(const char *title_id, const uint8_t *elf,
                          size_t elf_sz, void *user);
  void *prepare_user;

  /** Bootstrapper: auto-delete obsolete Error disabler plugin. */
  int auto_delete_eorr37000;

  /**
   * Bootstrapper: return true after a launch attempt even if pid < 0.
   * Util: 0 — require pid >= 0.
   */
  int always_succeed_after_launch;
} orion_plugin_load_opts;

/**
 * Load a .elf or .plugin from @path.
 * @filename Optional basename (e.g. for extension checks). If NULL, derived
 *           from @path (basename may modify a temporary copy only).
 */
bool orion_plugin_load(const char *path, const char *filename,
                       const orion_plugin_load_opts *opts);

#ifdef __cplusplus
}
#endif
