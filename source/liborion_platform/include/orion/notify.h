/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Shared system notification toast.
 *
 * Use orion_notify(show_wm, fmt, ...) everywhere for the watermark form.
 * ShellUI keeps a separate notify(const char*, ...) overload in shellui_notify.cpp
 * (string literals would be ambiguous with a bool-first overload).
 *
 * Pure C call sites may use:  #define notify(wm, ...) orion_notify((wm), __VA_ARGS__)
 *
 * IMPORTANT (ShellUI injectees):
 *   Do NOT call sceKernelSendNotificationRequest by name from this library.
 *   Injected payloads resolve that symbol as a *function pointer* via dlsym;
 *   a direct CALL would execute the pointer's storage as code (SIGSEGV/SIGILL).
 *   Hosts must register the real sender with orion_notify_set_send().
 */

#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Kernel/userland toast send — matches sceKernelSendNotificationRequest. */
typedef int32_t (*orion_notify_send_fn)(int32_t device, void *req, size_t size,
                                        int32_t blocking);

/**
 * Install the send implementation for this process.
 * - util/daemon/bootstrapper: pass the real linked sceKernelSendNotificationRequest
 * - shellui/fps_elf: pass a trampoline that calls through the dlsym'd pointer
 * Must be called before any orion_notify / notify().
 */
void orion_notify_set_send(orion_notify_send_fn fn);

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
