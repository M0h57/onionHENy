/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * ShellUI notify(const char *, ...) — no watermark bool (historical UI API).
 */

#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include <orion/notify.h>
#include <cstdarg>
#include <cstdio>

void notify(const char *text, ...) {
  va_list args{};
  va_start(args, text);
  // show_watermark ignored by format; still prefixes [OrionHEN] like daemons.
  // Send goes through orion_notify_set_send trampoline (dlsym pointer safe).
  orion_notify_v(/*show_watermark=*/0, text, args);
  va_end(args);
  shellui_log("Notify send returned (orion_notify path)");
}
