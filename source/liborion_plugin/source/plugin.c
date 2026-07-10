/* Copyright (C) 2025 OrionHEN / LightningMods */

#include <orion/plugin.h>

#include <elfldr_remote.h>
#include <orion/log.h>
#include <orion/notify.h>
#include <orion/proc_query.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern int sceKernelGetProcessName(int pid, char *name);

bool orion_plugin_is_elf(const void *buf, size_t size) {
  if (!buf || size < 4)
    return false;
  static const unsigned char kMagic[] = {0x7F, 'E', 'L', 'F'};
  return memcmp(buf, kMagic, 4) == 0;
}

bool orion_plugin_is_valid(const void *buf) {
  if (!buf)
    return false;

  if (strncmp((const char *)buf, "OrionHEN_PLUGIN", 14) != 0) {
    OrionHEN_log("Plugin header prefix does not match");
    return false;
  }

  const OrionPluginHeader *header = (const OrionPluginHeader *)buf;
  for (int i = 0; i < 4; ++i) {
    if (header->titleID[i] < 'A' || header->titleID[i] > 'Z') {
      OrionHEN_log(
          "Invalid plugin file: titleID must contain 4 uppercase letters "
          "as the start");
      return false;
    }
  }
  for (int i = 4; i < 9; ++i) {
    if (header->titleID[i] < '0' || header->titleID[i] > '9') {
      OrionHEN_log(
          "Invalid plugin file: titleID must contain 5 numbers as the end");
      return false;
    }
  }
  if (header->titleID[9] != '\0') {
    OrionHEN_log("Invalid plugin file: titleID must be null-terminated");
    return false;
  }

  for (int i = 0; i < 3; ++i) {
    if (header->plugin_version[i] == '.')
      continue;
    if (header->plugin_version[i] < '0' || header->plugin_version[i] > '9') {
      OrionHEN_log(
          "Invalid plugin file: version must be in the following format xx.xx");
      return false;
    }
  }
  return true;
}

const uint8_t *orion_plugin_package_elf(const void *buf) {
  return (const uint8_t *)buf + sizeof(OrionPluginHeader);
}

void orion_plugin_pid_path(char *out, size_t out_sz, const char *title_id) {
  snprintf(out, out_sz, "/system_tmp/%s.PID", title_id);
}

pid_t orion_plugin_read_pid_file(const char *pid_path) {
  const int f = open(pid_path, O_RDONLY);
  if (f < 0)
    return -1;
  char t[32];
  const int r = (int)read(f, t, sizeof(t) - 1);
  close(f);
  if (r <= 0)
    return -1;
  t[r] = '\0';
  return (pid_t)atoi(t);
}

void orion_plugin_write_pid_file(const char *pid_path, pid_t pid) {
  const int f = open(pid_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (f < 0)
    return;
  if (pid >= 0) {
    char t[32];
    const int len = snprintf(t, sizeof(t), "%d", pid);
    (void)write(f, t, (size_t)len);
  } else {
    unlink(pid_path);
  }
  close(f);
}

void orion_plugin_stop_by_title(const char *title_id) {
  char pid_path[256];
  orion_plugin_pid_path(pid_path, sizeof(pid_path), title_id);

  pid_t pid = orion_plugin_read_pid_file(pid_path);
  if (pid > 0) {
    char name[32];
    if (sceKernelGetProcessName(pid, name) < 0) {
      OrionHEN_log("Stale plugin PID file detected for %s, removing", title_id);
      unlink(pid_path);
      pid = -1;
    }
  }

  if (pid > 0) {
    OrionHEN_log("killing pid %d (plugin: %s)", pid, title_id);
    if (kill(pid, SIGKILL) != 0)
      OrionHEN_log("kill(%d) failed: %s", pid, strerror(errno));
    unlink(pid_path);
  }
}

pid_t orion_plugin_launch_9021(const char *title_id, const uint8_t *elf,
                               size_t elf_sz) {
  char epath[256];
  snprintf(epath, sizeof(epath), "/data/OrionHEN/plugins/%s.elf", title_id);
  OrionHEN_log("loading plugin via 9021 title=%s path=%s", title_id, epath);

  if (!elfldr_remote_write_and_launch(epath, elf, elf_sz)) {
    OrionHEN_log("  Failed 9021 launch for %s", title_id);
    return -1;
  }

  sleep(2);

  /* Prefer exact name TITLE.elf, then substring title, else "unknown but ok". */
  char nbuf[64];
  snprintf(nbuf, sizeof(nbuf), "%s.elf", title_id);
  pid_t pid = find_pid(nbuf);
  if (pid < 0)
    pid = find_pid(title_id);
  if (pid < 0)
    pid = orion_find_pid_substr(title_id);
  if (pid < 0)
    pid = 1;

  OrionHEN_log("  Launched via 9021 (pid=%d)", (int)pid);
  return pid;
}

uint8_t *orion_plugin_read_file(const char *path, size_t *out_size) {
  if (!path || !out_size)
    return NULL;

  const int fd = open(path, O_RDONLY);
  if (fd < 0) {
    OrionHEN_log("Failed to open file %s (%s)", path, strerror(errno));
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    OrionHEN_log("Failed to stat file %s", path);
    close(fd);
    return NULL;
  }
  if (st.st_size <= 0) {
    OrionHEN_log("Empty plugin file %s", path);
    close(fd);
    return NULL;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)st.st_size);
  if (!buf) {
    OrionHEN_log("Failed to allocate %lld bytes for plugin",
                 (long long)st.st_size);
    close(fd);
    return NULL;
  }

  if (read(fd, buf, (size_t)st.st_size) != st.st_size) {
    OrionHEN_log("Failed to read plugin file %s", path);
    free(buf);
    close(fd);
    return NULL;
  }
  close(fd);
  *out_size = (size_t)st.st_size;
  return buf;
}

bool orion_plugin_load(const char *path, const char *filename,
                       const orion_plugin_load_opts *opts) {
  orion_plugin_load_opts local = {0};
  if (opts)
    local = *opts;

  size_t size = 0;
  uint8_t *buf = orion_plugin_read_file(path, &size);
  if (!buf)
    return false;

  char name_buf[256];
  const char *base = filename;
  if (!base || !base[0]) {
    /* basename may mutate; work on a copy */
    snprintf(name_buf, sizeof(name_buf), "%s", path);
    base = basename(name_buf);
  }

  const OrionPluginHeader *header = (const OrionPluginHeader *)buf;
  char pid_path[256];
  orion_plugin_pid_path(pid_path, sizeof(pid_path), header->titleID);

  /* ---- raw .elf ---- */
  if (strstr(base, ".elf") != NULL) {
    OrionHEN_log("ELF detected: %s", base);
    if (!orion_plugin_is_elf(buf, size)) {
      OrionHEN_log("Invalid ELF file: %s", base);
      orion_notify(1, "Invalid ELF file: %s", base);
      free(buf);
      return false;
    }

    orion_plugin_stop_by_title(header->titleID);
    const pid_t pid = orion_plugin_launch_9021(header->titleID, buf, size);
    free(buf);
    orion_plugin_write_pid_file(pid_path, pid);
    if (local.always_succeed_after_launch)
      return true;
    return pid >= 0;
  }

  /* ---- .plugin package ---- */
  if (!orion_plugin_is_valid(buf)) {
    free(buf);
    return false;
  }

  OrionHEN_log("============== Plugin info ===============");
  OrionHEN_log("Plugin Prefix: %s", header->prefix);
  OrionHEN_log("Plugin TitleID: %s", header->titleID);
  OrionHEN_log("Plugin Version: %s", header->plugin_version);
  OrionHEN_log("=========================================");

  orion_plugin_stop_by_title(header->titleID);

  if (local.auto_delete_eorr37000 &&
      strcmp(header->titleID, "EORR37000") == 0) {
    orion_notify(
        1,
        "The Error disabler plugin is no longer required and has been auto "
        "deleted.");
    unlink(path);
    free(buf);
    return true;
  }

  const uint8_t *elf = orion_plugin_package_elf(buf);
  const size_t elf_sz = size - sizeof(OrionPluginHeader);

  if (local.prepare_package)
    local.prepare_package(header->titleID, elf, elf_sz, local.prepare_user);

  const pid_t pid = orion_plugin_launch_9021(header->titleID, elf, elf_sz);
  orion_plugin_write_pid_file(pid_path, pid);
  free(buf);

  if (local.always_succeed_after_launch)
    return true;
  return pid >= 0;
}
