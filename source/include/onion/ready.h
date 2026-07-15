/* Copyright (C) 2025 OnionHEN / LightningMods

Cross-process readiness protocol for OnionHEN services.

Services publish a named marker under /system_tmp/onion_ready/<name>.
Consumers wait with timeout instead of fixed sleep() races.

Legacy alias: "toolbox" also checks /system_tmp/toolbox_online (and signals it).
*/

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ONION_READY_ROOT "/system_tmp/onion_ready"

/* Well-known service names (startup orchestration) */
#define ONION_READY_UTIL "util"
#define ONION_READY_KSTUFF "kstuff"
#define ONION_READY_DAEMON "daemon"
#define ONION_READY_TOOLBOX "toolbox"

/*
 * Runtime flags (same storage as ready markers; typed names replace ad-hoc
 * /system_tmp flag files).
 *   fps_overlay  — shellui wants FPS inject when a CUSA/SCUS game is running
 *   util_booted  — util finished at least one full start (mid-session restart detect)
 */
#define ONION_FLAG_FPS_OVERLAY "fps_overlay"
#define ONION_FLAG_UTIL_BOOTED "util_booted"

/* Publish / clear / query. name is a short token (no path separators). */
bool onion_ready_signal(const char *name);
bool onion_ready_clear(const char *name);
bool onion_ready_is_set(const char *name);

/*
 * Poll until marker is set or timeout_ms elapses.
 * poll_ms is the sleep between checks (clamped to >= 50).
 * Returns true if ready, false on timeout or invalid name.
 */
bool onion_ready_wait(const char *name, int timeout_ms, int poll_ms);

/* Build absolute path for a name into buf (for logging). */
bool onion_ready_path(const char *name, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
