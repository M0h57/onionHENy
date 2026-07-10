/* Copyright (C) 2025 OrionHEN / LightningMods

Cross-process readiness protocol for OrionHEN services.

Services publish a named marker under /system_tmp/orion_ready/<name>.
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

#define ORION_READY_ROOT "/system_tmp/orion_ready"

/* Well-known service names (startup orchestration) */
#define ORION_READY_UTIL "util"
#define ORION_READY_KSTUFF "kstuff"
#define ORION_READY_DAEMON "daemon"
#define ORION_READY_TOOLBOX "toolbox"

/*
 * Runtime flags (same storage as ready markers; typed names replace ad-hoc
 * /system_tmp/*.flag files).
 *   fps_overlay  — shellui wants FPS inject when a CUSA/SCUS game is running
 *   util_booted  — util finished at least one full start (rest-mode / delay)
 */
#define ORION_FLAG_FPS_OVERLAY "fps_overlay"
#define ORION_FLAG_UTIL_BOOTED "util_booted"

/* Publish / clear / query. name is a short token (no path separators). */
bool orion_ready_signal(const char *name);
bool orion_ready_clear(const char *name);
bool orion_ready_is_set(const char *name);

/*
 * Poll until marker is set or timeout_ms elapses.
 * poll_ms is the sleep between checks (clamped to >= 50).
 * Returns true if ready, false on timeout or invalid name.
 */
bool orion_ready_wait(const char *name, int timeout_ms, int poll_ms);

/* Build absolute path for a name into buf (for logging). */
bool orion_ready_path(const char *name, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif
