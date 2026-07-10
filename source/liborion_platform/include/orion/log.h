/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Shared OrionHEN_log: stdout + klog + optional file.
 * Configure once per binary (util vs daemon paths/tags).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure log tag and optional file path.
 * @param tag       e.g. "OrionHEN" or "OrionHEN utils" (printed as [tag]: )
 * @param log_path  absolute path, or NULL to disable file sink
 */
void orion_log_configure(const char *tag, const char *log_path);

/** Primary product log API (keeps historical name for call-site compatibility). */
void OrionHEN_log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
