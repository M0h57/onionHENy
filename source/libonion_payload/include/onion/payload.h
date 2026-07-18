/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Payload ELF load helpers (PID files + elfldr socket).
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

/** Kill process recorded in /system_tmp/onionhen/pid/<key>.PID if still valid. */
void onion_payload_stop_by_title(const char *title_id);

/**
 * Stage ELF under /data/OnionHEN/payloads/<key>.elf and launch via OnionHEN's
 * private 9020 elfldr, falling back to the external 9021 loader.
 *
 * Records the real process by snapshot-diff of candidate ki_comm names
 * (payload.elf, <key>.elf, truncated basename, <key>) so homebrew that never
 * thr_set_name still gets a correct PID file for later kill.
 *
 * Returns:
 *   >1  observed payload pid (write to /system_tmp/onionhen/pid/<key>.PID)
 *    0  loader accepted the ELF but no new pid observed (do not write 0/1)
 *   -1  hard failure (send/stage failed)
 * Never returns 1 as a "success" sentinel (that caused ForceKill of system pid 1).
 */
pid_t onion_payload_launch_elfldr(const char *title_id, const uint8_t *elf,
                                  size_t elf_sz);

/** Backward-compatible API name; now prefers 9020 and falls back to 9021. */
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
