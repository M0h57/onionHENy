/* Copyright (C) 2025 OrionHEN / LightningMods */

#include <orion/ready.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(ORION_HOST_TEST)
#define ORION_READY_ROOT_HOST "/tmp/orion_ready"
#undef ORION_READY_ROOT
#define ORION_READY_ROOT ORION_READY_ROOT_HOST
#define LEGACY_TOOLBOX_ONLINE "/tmp/toolbox_online"
#else
#define LEGACY_TOOLBOX_ONLINE "/system_tmp/toolbox_online"
#endif

static bool valid_name(const char *name) {
  if (!name || !*name) {
    return false;
  }
  for (const char *p = name; *p; ++p) {
    if (*p == '/' || *p == '.' || *p == '\\') {
      return false;
    }
  }
  return true;
}

bool orion_ready_path(const char *name, char *buf, size_t buflen) {
  if (!valid_name(name) || !buf || buflen < 32) {
    return false;
  }
  int n = snprintf(buf, buflen, "%s/%s", ORION_READY_ROOT, name);
  return n > 0 && (size_t)n < buflen;
}

static void ensure_root(void) {
  mkdir(ORION_READY_ROOT, 0777);
}

static bool touch_path(const char *path) {
  int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (fd < 0) {
    return false;
  }
  (void)write(fd, "1", 1);
  close(fd);
  return true;
}

static bool path_exists(const char *path) {
  struct stat st;
  return path && stat(path, &st) == 0;
}

bool orion_ready_signal(const char *name) {
  char path[256];
  if (!orion_ready_path(name, path, sizeof(path))) {
    return false;
  }
  ensure_root();
  bool ok = touch_path(path);
  /* Keep legacy marker for older daemons / tooling */
  if (ok && strcmp(name, ORION_READY_TOOLBOX) == 0) {
    (void)touch_path(LEGACY_TOOLBOX_ONLINE);
  }
  return ok;
}

bool orion_ready_clear(const char *name) {
  char path[256];
  if (!orion_ready_path(name, path, sizeof(path))) {
    return false;
  }
  unlink(path);
  if (strcmp(name, ORION_READY_TOOLBOX) == 0) {
    unlink(LEGACY_TOOLBOX_ONLINE);
  }
  return true;
}

bool orion_ready_is_set(const char *name) {
  char path[256];
  if (!orion_ready_path(name, path, sizeof(path))) {
    return false;
  }
  if (path_exists(path)) {
    return true;
  }
  /* Accept legacy toolbox marker as ready */
  if (strcmp(name, ORION_READY_TOOLBOX) == 0 &&
      path_exists(LEGACY_TOOLBOX_ONLINE)) {
    return true;
  }
  return false;
}

bool orion_ready_wait(const char *name, int timeout_ms, int poll_ms) {
  if (!valid_name(name)) {
    return false;
  }
  if (timeout_ms < 0) {
    timeout_ms = 0;
  }
  if (poll_ms < 50) {
    poll_ms = 50;
  }

  int waited = 0;
  while (waited <= timeout_ms) {
    if (orion_ready_is_set(name)) {
      return true;
    }
    if (waited >= timeout_ms) {
      break;
    }
    usleep((useconds_t)poll_ms * 1000);
    waited += poll_ms;
  }
  return orion_ready_is_set(name);
}
