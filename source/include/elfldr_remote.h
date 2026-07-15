/* OnionHEN: launch ELFs via external elfldr on TCP 9021 (no local spawn). */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ELFLDR_REMOTE_PORT
#define ELFLDR_REMOTE_PORT 9021
#endif

/**
 * True if something accepts connections on 127.0.0.1:9021 (typically elfldr.elf).
 */
bool elfldr_remote_available(void);

/**
 * Send raw ELF bytes to 9021 (process name becomes "payload.elf").
 */
bool elfldr_remote_send_bytes(const uint8_t *elf, size_t size);

/**
 * Ask elfldr to load a local file via "file:/path\n" URI.
 * Basename becomes the process name (e.g. util.elf, CUSA12345.elf).
 */
bool elfldr_remote_send_file_uri(const char *abs_path);

/**
 * Write @elf to @abs_path then launch via file: URI.
 * Creates parent directories best-effort (mkdir of immediate parent only).
 */
bool elfldr_remote_write_and_launch(const char *abs_path,
                                    const uint8_t *elf, size_t size);

/**
 * Poll for a process whose ki_comm contains @name_substr (same idea as find_pid).
 * Returns pid or -1. @timeout_ms total wait.
 */
pid_t elfldr_remote_wait_name(const char *name_substr, int timeout_ms);

#ifdef __cplusplus
}
#endif
