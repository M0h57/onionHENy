/* OnionHEN: launch ELFs via elfldr sockets.
 *
 * 9020 is OnionHEN's private embedded loader. 9021 is the legacy/external
 * ps5-payload-dev elfldr port kept as a bootstrap and compatibility fallback.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ELFLDR_REMOTE_PORT
#define ELFLDR_REMOTE_PORT 9021
#endif

#ifndef ONION_ELFLDR_PORT
#define ONION_ELFLDR_PORT 9020
#endif

#define ONION_ELFLDR_PING "onion:ping"
#define ONION_ELFLDR_PONG "OK onion_elfldr 9020\n"

/**
 * True if something accepts connections on 127.0.0.1:@port.
 */
bool elfldr_remote_available_on(uint16_t port);

/**
 * True only if OnionHEN's private embedded loader answers on 127.0.0.1:9020.
 */
bool elfldr_remote_onion_available(void);

/**
 * True if something accepts connections on 127.0.0.1:9021 (typically elfldr.elf).
 */
bool elfldr_remote_available(void);

/**
 * Send raw ELF bytes to @port (process name becomes "payload.elf" unless the
 * payload renames itself).
 */
bool elfldr_remote_send_bytes_to(uint16_t port, const uint8_t *elf,
                                 size_t size);

/**
 * Send raw ELF bytes to 9021 (legacy external elfldr).
 */
bool elfldr_remote_send_bytes(const uint8_t *elf, size_t size);

/**
 * Ask elfldr at @port to load a local file via "file:/path\n" URI.
 * Basename becomes the process name (e.g. util.elf, CUSA12345.elf).
 */
bool elfldr_remote_send_file_uri_to(uint16_t port, const char *abs_path);

/**
 * Ask legacy external elfldr to load a local file via "file:/path\n" URI.
 */
bool elfldr_remote_send_file_uri(const char *abs_path);

/**
 * Write @elf to @abs_path then launch through @port via file: URI.
 * Creates parent directories best-effort (mkdir of immediate parent only).
 */
bool elfldr_remote_write_and_launch_to(uint16_t port, const char *abs_path,
                                       const uint8_t *elf, size_t size);

/**
 * Same as elfldr_remote_write_and_launch_to(), but waits for an OnionHEN 9020
 * server response. Returns:
 *   >1  payload pid reported by the embedded loader
 *    0  accepted but no pid was returned
 *   -1  send/stage failure
 */
pid_t elfldr_remote_write_and_launch_get_pid(uint16_t port,
                                             const char *abs_path,
                                             const uint8_t *elf, size_t size);

/**
 * Write @elf to @abs_path then launch via legacy 9021 file: URI.
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
