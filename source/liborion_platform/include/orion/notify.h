/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Shared system notification toast.
 *
 * Use orion_notify(show_wm, fmt, ...) everywhere for the watermark form.
 * ShellUI keeps a separate notify(const char*, ...) overload in shellui_notify.cpp
 * (string literals would be ambiguous with a bool-first overload).
 *
 * Pure C call sites may use:  #define notify(wm, ...) orion_notify((wm), __VA_ARGS__)
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void orion_notify_v(int show_watermark, const char *fmt, va_list ap);
void orion_notify(int show_watermark, const char *fmt, ...);

void orion_notify_format(char *out, size_t out_sz, int show_watermark,
                         const char *fmt, va_list ap);

#ifndef __cplusplus
/* Historical C name — not defined in C++ (avoids overload ambiguity). */
#define notify(show_wm, ...) orion_notify((show_wm), __VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif
