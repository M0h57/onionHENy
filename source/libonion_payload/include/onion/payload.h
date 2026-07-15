/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Payload ELF load helpers (PID files + elfldr :9021).
 * OnionHEN only supports bare .elf payloads (no .plugin packages).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

bool onion_payload_is_elf(const void *buf, size_t size);

/**
 * Derive launch/PID key from basename: "foo.elf" → "foo".
 * Rejects empty stem (".elf") and path separators.
 */
bool onion_payload_elf_key_from_name(const char *name, char *out, size_t out_sz);

void onion_payload_pid_path(char *out, size_t out_sz, const char *title_id);
pid_t onion_payload_read_pid_file(const char *pid_path);
void onion_payload_write_pid_file(const char *pid_path, pid_t pid);

/** Kill process recorded in /system_tmp/<key>.PID if still valid. */
void onion_payload_stop_by_title(const char *title_id);

/**
 * Stage ELF under /data/OnionHEN/payloads/<key>.elf and launch via 9021.
 * On success returns pid (>=0). If process name is not observed, returns 1.
 * On failure returns -1.
 */
pid_t onion_payload_launch_9021(const char *title_id, const uint8_t *elf,
                               size_t elf_sz);

/** malloc'd file contents; caller free(). NULL on error. */
uint8_t *onion_payload_read_file(const char *path, size_t *out_size);

typedef struct onion_payload_load_opts {
  /**
   * Bootstrapper: return true after a launch attempt even if pid < 0.
   * Util: 0 — require pid >= 0.
   */
  int always_succeed_after_launch;
} onion_payload_load_opts;

/**
 * Load a payload .elf from @path.
 * @filename Optional basename. If NULL, derived from @path.
 */
bool onion_payload_load(const char *path, const char *filename,
                       const onion_payload_load_opts *opts);

#ifdef __cplusplus
}
#endif
