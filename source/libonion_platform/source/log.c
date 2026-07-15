/* Copyright (C) 2025 OnionHEN / LightningMods */

#include <onion/log.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* klog_printf provided by PS5 payload runtime / linked stubs. */
void klog_printf(const char *fmt, ...);

static char g_tag[64] = "OnionHEN";
static char g_log_path[256] = "";

void onion_log_configure(const char *tag, const char *log_path) {
  if (tag && tag[0]) {
    snprintf(g_tag, sizeof(g_tag), "%s", tag);
  }
  if (log_path && log_path[0]) {
    snprintf(g_log_path, sizeof(g_log_path), "%s", log_path);
  } else {
    g_log_path[0] = '\0';
  }
}

void OnionHEN_log(const char *fmt, ...) {
  char msg[0x1000];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  size_t msg_len = strlen(msg);
  if (msg_len < sizeof(msg) - 1) {
    msg[msg_len] = '\n';
    msg[msg_len + 1] = '\0';
  } else {
    msg[sizeof(msg) - 2] = '\n';
    msg[sizeof(msg) - 1] = '\0';
  }

  printf("[%s]: %s", g_tag, msg);
  klog_printf("%s", msg);

  if (g_log_path[0] == '\0') {
    return;
  }
  int fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND, 0777);
  if (fd < 0) {
    return;
  }
  write(fd, msg, strlen(msg));
  close(fd);
}
