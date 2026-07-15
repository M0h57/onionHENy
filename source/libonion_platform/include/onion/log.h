/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Shared OnionHEN_log: stdout + klog + optional file.
 * Configure once per binary (util vs daemon paths/tags).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Configure log tag and optional file path.
 * @param tag       e.g. "OnionHEN" or "OnionHEN utils" (printed as [tag]: )
 * @param log_path  absolute path, or NULL to disable file sink
 */
void onion_log_configure(const char *tag, const char *log_path);

/** Primary product log API (keeps historical name for call-site compatibility). */
void OnionHEN_log(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
