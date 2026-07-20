/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Process-local localization for user-facing notification text.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum onion_notify_language {
  ONION_NOTIFY_LANG_ZH_HANS = 0,
  ONION_NOTIFY_LANG_EN = 1,
} onion_notify_language_t;

/** Query callback matching sceSystemServiceParamGetInt. */
typedef int (*onion_notify_system_language_query_fn)(int param_id, int *value);

/**
 * Register the process-safe system language query implementation.
 * Registration invalidates the process-local system language cache.
 */
void onion_notify_set_system_language_query(
    onion_notify_system_language_query_fn query);

/** Invalidate the cached raw PS5 system language. */
void onion_notify_invalidate_system_language(void);

/** Refresh and return the raw PS5 system language (English fallback on error). */
int onion_notify_refresh_system_language(void);

/** Return the cached raw PS5 language, querying at most once when uncached. */
int onion_notify_cached_system_language(void);

/** Select the resolved language used by plain and rich notifications. */
void onion_notify_set_language(onion_notify_language_t language);

/** Return the currently selected notification language. */
onion_notify_language_t onion_notify_get_language(void);

/**
 * Resolve the shared toolbox language setting.
 * ui_language: 0=system, 1=zh-Hans, 2=en.
 * system_language is the value returned for SCE_SYSTEM_SERVICE_PARAM_ID_LANG.
 */
onion_notify_language_t onion_notify_resolve_language(int ui_language,
                                                       int system_language);

/** Select a language from the shared setting and PS5 system language. */
void onion_notify_apply_ui_language(int ui_language, int system_language);

/** Resolve and apply a UI language using the process-local system cache. */
void onion_notify_apply_ui_language_cached(int ui_language);

/** Translate an English notification format string; unknown text is unchanged. */
const char *onion_notify_tr(const char *english);

#ifdef __cplusplus
}
#endif
