/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Payload ELF loader (PID files + elfldr socket). .plugin packages removed.
 */

#include <onion/payload.h>

#include <elfldr_remote.h>
#include <onion/log.h>
#include <onion/notify.h>
#include <onion/proc_query.h>
#include <onion/system_tmp.h>

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

bool onion_payload_is_elf(const void *buf, size_t size) {
  if (!buf || size < 4)
    return false;
  static const unsigned char kMagic[] = {0x7F, 'E', 'L', 'F'};
  return memcmp(buf, kMagic, 4) == 0;
}

bool onion_payload_elf_key_from_name(const char *name, char *out, size_t out_sz) {
  if (!name || !out || out_sz < 2)
    return false;

  const char *base = strrchr(name, '/');
  base = base ? base + 1 : name;
  if (!base[0] || strcmp(base, ".") == 0 || strcmp(base, "..") == 0)
    return false;

  size_t n = strlen(base);
  if (n >= 4 && strcmp(base + n - 4, ".elf") == 0)
    n -= 4;
  if (n == 0)
    return false; /* bare ".elf" */
  if (n >= out_sz)
    n = out_sz - 1;

  memcpy(out, base, n);
  out[n] = '\0';
  if (out[0] == '\0' || strchr(out, '/') != NULL || strchr(out, '\\') != NULL)
    return false;
  return true;
}

void onion_payload_pid_path(char *out, size_t out_sz, const char *title_id) {
  (void)onion_system_tmp_pid_path(out, out_sz, title_id);
}

pid_t onion_payload_read_pid_file(const char *pid_path) {
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

void onion_payload_write_pid_file(const char *pid_path, pid_t pid) {
  if (!pid_path) {
    return;
  }
  if (pid < 0) {
    unlink(pid_path);
    return;
  }

  mkdir(ONION_SYSTEM_TMP_ROOT, 0777);
  mkdir(ONION_SYSTEM_TMP_PID_ROOT, 0777);
  const int f = open(pid_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
  if (f < 0)
    return;
  char t[32];
  const int len = snprintf(t, sizeof(t), "%d", pid);
  (void)write(f, t, (size_t)len);
  close(f);
}

/*
 * elfldr process-name rules (see elfldr_remote.h):
 *   - raw bytes over the socket → often "payload.elf"
 *   - file:/path URI      → basename (e.g. web-file-mgr-v0.8.elf)
 * Orbis ki_comm is only COMMLEN (19) chars — long basenames truncate.
 *
 * Kill path must prefer the PID recorded at launch (PID file). Name-only
 * lookup is ambiguous for "payload.elf" when several homebrews run.
 */

#ifndef COMMLEN
#define COMMLEN 19
#endif

#define PAYLOAD_PID_SNAP_MAX 64

/** Build the set of ki_comm strings this launch may appear under. */
static size_t onion_payload_candidate_names(const char *title_id, char *elf_name,
                                            size_t elf_name_sz, char *trunc_name,
                                            size_t trunc_sz,
                                            const char **names, size_t max_names) {
  size_t n = 0;
  if (!title_id || !title_id[0] || !names || max_names == 0)
    return 0;

  /* Most common for tools that never thr_set_name. */
  if (n < max_names)
    names[n++] = "payload.elf";

  if (elf_name && elf_name_sz > 0) {
    snprintf(elf_name, elf_name_sz, "%s.elf", title_id);
    if (n < max_names)
      names[n++] = elf_name;

    /* Truncated form when basename > COMMLEN (ki_comm hard limit). */
    if (trunc_name && trunc_sz > 0 && strlen(elf_name) > (size_t)COMMLEN) {
      size_t copy = (size_t)COMMLEN;
      if (copy >= trunc_sz)
        copy = trunc_sz - 1;
      memcpy(trunc_name, elf_name, copy);
      trunc_name[copy] = '\0';
      if (n < max_names)
        names[n++] = trunc_name;
    }
  }

  if (n < max_names)
    names[n++] = title_id;

  return n;
}

static bool onion_payload_pid_in_list(pid_t pid, const pid_t *list, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    if (list[i] == pid)
      return true;
  }
  return false;
}

/**
 * Name-based resolve for kill fallback only.
 * Prefer title-specific names; avoid bare "payload.elf" (ambiguous).
 */
static pid_t onion_payload_resolve_pid_by_title(const char *title_id) {
  if (!title_id || !title_id[0])
    return -1;

  char nbuf[64];
  char trunc[COMMLEN + 1];
  snprintf(nbuf, sizeof(nbuf), "%s.elf", title_id);

  pid_t pid = find_pid(nbuf);
  if (pid <= 1 && strlen(nbuf) > (size_t)COMMLEN) {
    memcpy(trunc, nbuf, COMMLEN);
    trunc[COMMLEN] = '\0';
    pid = find_pid(trunc);
  }
  if (pid <= 1)
    pid = find_pid(title_id);
  if (pid <= 1)
    pid = onion_find_pid_substr(title_id);
  if (pid <= 1)
    return -1;
  return pid;
}

/**
 * After a loader accepts launch: find a NEW pid among candidate names not in
 * @before. Picks the highest new pid (typically the most recently created).
 */
static pid_t onion_payload_find_new_pid(const char *title_id,
                                        const pid_t *before, size_t n_before) {
  char elf_name[64];
  char trunc_name[COMMLEN + 1];
  const char *names[8];
  const size_t nnames = onion_payload_candidate_names(
      title_id, elf_name, sizeof(elf_name), trunc_name, sizeof(trunc_name),
      names, sizeof(names) / sizeof(names[0]));

  pid_t after[PAYLOAD_PID_SNAP_MAX];
  const size_t n_after =
      onion_collect_pids(names, nnames, after, PAYLOAD_PID_SNAP_MAX);

  pid_t best = -1;
  for (size_t i = 0; i < n_after; ++i) {
    if (after[i] <= 1)
      continue;
    if (onion_payload_pid_in_list(after[i], before, n_before))
      continue;
    if (after[i] > best)
      best = after[i];
  }
  return best;
}

void onion_payload_stop_by_title(const char *title_id) {
  char pid_path[256];
  onion_payload_pid_path(pid_path, sizeof(pid_path), title_id);

  /* Primary: kill the pid we recorded at launch. */
  pid_t pid = onion_payload_read_pid_file(pid_path);
  /* PID 0/1 are never valid payload targets. */
  if (pid <= 1) {
    if (pid == 1) {
      OnionHEN_log("Ignoring bogus payload PID 1 for %s", title_id);
      unlink(pid_path);
    }
    pid = -1;
  } else if (!onion_proc_is_alive(pid)) {
    OnionHEN_log("Stale payload PID file for %s (pid=%d dead), removing",
                 title_id, (int)pid);
    unlink(pid_path);
    pid = -1;
  } else {
    char name[32];
    if (sceKernelGetProcessName(pid, name) < 0) {
      OnionHEN_log("Stale payload PID file detected for %s, removing", title_id);
      unlink(pid_path);
      pid = -1;
    }
  }

  /* Secondary: title-specific process name only (not generic payload.elf). */
  if (pid <= 1)
    pid = onion_payload_resolve_pid_by_title(title_id);

  if (pid > 1) {
    OnionHEN_log("killing pid %d (payload: %s)", (int)pid, title_id);
    if (kill(pid, SIGKILL) != 0)
      OnionHEN_log("kill(%d) failed: %s", (int)pid, strerror(errno));
    unlink(pid_path);
  } else {
    OnionHEN_log("stop_by_title: no live pid for %s", title_id);
  }
}

pid_t onion_payload_launch_elfldr(const char *title_id, const uint8_t *elf,
                                  size_t elf_sz) {
  if (!title_id || !title_id[0] || !elf || elf_sz < 4) {
    OnionHEN_log("launch_elfldr: invalid args title=%s elf_sz=%zu",
                 title_id ? title_id : "(null)", elf_sz);
    return -1;
  }
  if (strcmp(title_id, ".") == 0 || strcmp(title_id, "..") == 0 ||
      strchr(title_id, '/') != NULL) {
    OnionHEN_log("launch_elfldr: rejected title_id=%s", title_id);
    return -1;
  }

  mkdir("/data/OnionHEN", 0777);
  mkdir("/data/OnionHEN/payloads", 0777);

  char epath[256];
  snprintf(epath, sizeof(epath), "/data/OnionHEN/payloads/%s.elf", title_id);
  OnionHEN_log("loading payload via elfldr key=%s path=%s", title_id, epath);

  /*
   * Snapshot candidate pids BEFORE launch so we can attribute a new
   * "payload.elf" (or basename) process to this launch, even when several
   * homebrews share the same ki_comm.
   */
  char elf_name[64];
  char trunc_name[COMMLEN + 1];
  const char *names[8];
  const size_t nnames = onion_payload_candidate_names(
      title_id, elf_name, sizeof(elf_name), trunc_name, sizeof(trunc_name),
      names, sizeof(names) / sizeof(names[0]));

  pid_t before[PAYLOAD_PID_SNAP_MAX];
  const size_t n_before =
      onion_collect_pids(names, nnames, before, PAYLOAD_PID_SNAP_MAX);
  OnionHEN_log("  pre-launch snapshot: %zu candidate pid(s) "
               "(names include payload.elf / %s)",
               n_before, elf_name);

  pid_t reported_pid = -1;
  uint16_t used_port = 0;
  if (elfldr_remote_onion_available()) {
    reported_pid = elfldr_remote_write_and_launch_get_pid(
        ONION_ELFLDR_PORT, epath, elf, elf_sz);
    if (reported_pid >= 0) {
      used_port = ONION_ELFLDR_PORT;
    } else {
      OnionHEN_log("  9020 launch failed for %s; trying external 9021",
                   title_id);
    }
  }

  if (used_port == 0) {
    if (!elfldr_remote_write_and_launch_to(ELFLDR_REMOTE_PORT, epath, elf,
                                           elf_sz)) {
      OnionHEN_log("  Failed elfldr launch for %s", title_id);
      return -1;
    }
    used_port = ELFLDR_REMOTE_PORT;
    reported_pid = 0;
  }

  if (reported_pid > 1) {
    char pname[32] = {0};
    if (sceKernelGetProcessName(reported_pid, pname) == 0)
      OnionHEN_log("  Launched via %u (pid=%d name=%s)", used_port,
                   (int)reported_pid, pname);
    else
      OnionHEN_log("  Launched via %u (pid=%d)", used_port,
                   (int)reported_pid);
    return reported_pid;
  }

  if (reported_pid < 0) {
    OnionHEN_log("  Failed elfldr launch for %s", title_id);
    return -1;
  }

  /*
   * Poll for a NEW pid among candidates. Never invent pid=1.
   *
   * Return codes:
   *   >1  real payload pid (caller writes PID file)
   *    0  loader accepted ELF but process not observed
   *   -1  hard failure
   */
  pid_t pid = -1;
  for (int attempt = 0; attempt < 25; ++attempt) {
    usleep(200 * 1000); /* 200ms × 25 ≈ 5s max */
    pid = onion_payload_find_new_pid(title_id, before, n_before);
    if (pid > 1)
      break;
  }

  if (pid > 1) {
    char pname[32] = {0};
    if (sceKernelGetProcessName(pid, pname) == 0)
      OnionHEN_log("  Launched via %u (pid=%d name=%s)", used_port, (int)pid,
                   pname);
    else
      OnionHEN_log("  Launched via %u (pid=%d)", used_port, (int)pid);
    return pid;
  }

  OnionHEN_log("  Launched via %u but new PID not observed for %s "
               "(no PID file; stop will try title name only)",
               used_port, title_id);
  return 0;
}

pid_t onion_payload_launch_9021(const char *title_id, const uint8_t *elf,
                                size_t elf_sz) {
  return onion_payload_launch_elfldr(title_id, elf, elf_sz);
}

uint8_t *onion_payload_read_file(const char *path, size_t *out_size) {
  if (!path || !out_size)
    return NULL;

  const int fd = open(path, O_RDONLY);
  if (fd < 0) {
    OnionHEN_log("Failed to open file %s (%s)", path, strerror(errno));
    return NULL;
  }

  struct stat st;
  if (fstat(fd, &st) != 0) {
    OnionHEN_log("Failed to stat file %s", path);
    close(fd);
    return NULL;
  }
  if (st.st_size <= 0) {
    OnionHEN_log("Empty payload file %s", path);
    close(fd);
    return NULL;
  }

  uint8_t *buf = (uint8_t *)malloc((size_t)st.st_size);
  if (!buf) {
    OnionHEN_log("Failed to allocate %lld bytes for payload",
                 (long long)st.st_size);
    close(fd);
    return NULL;
  }

  if (read(fd, buf, (size_t)st.st_size) != st.st_size) {
    OnionHEN_log("Failed to read payload file %s", path);
    free(buf);
    close(fd);
    return NULL;
  }
  close(fd);
  *out_size = (size_t)st.st_size;
  return buf;
}

bool onion_payload_load(const char *path, const char *filename,
                       const onion_payload_load_opts *opts) {
  onion_payload_load_opts local = {0};
  if (opts)
    local = *opts;

  size_t size = 0;
  uint8_t *buf = onion_payload_read_file(path, &size);
  if (!buf)
    return false;

  char name_buf[256];
  const char *base = filename;
  if (!base || !base[0]) {
    snprintf(name_buf, sizeof(name_buf), "%s", path);
    base = basename(name_buf);
  }

  const size_t base_len = strlen(base);
  if (!(base_len > 4 && strcmp(base + base_len - 4, ".elf") == 0)) {
    OnionHEN_log("Not a .elf payload: %s", base);
    onion_notify(1, "Only .elf payloads are supported:\n%s", base);
    free(buf);
    return false;
  }

  if (!onion_payload_is_elf(buf, size)) {
    OnionHEN_log("Invalid ELF file: %s", base);
    onion_notify(1, "Invalid ELF file: %s", base);
    free(buf);
    return false;
  }

  char key[64];
  if (!onion_payload_elf_key_from_name(base, key, sizeof(key))) {
    OnionHEN_log("Invalid ELF basename (empty stem): %s", base);
    onion_notify(1, "Invalid ELF name: %s", base);
    free(buf);
    return false;
  }

  OnionHEN_log("payload launch key=%s (from %s)", key, base);
  char pid_path[256];
  onion_payload_pid_path(pid_path, sizeof(pid_path), key);
  onion_payload_stop_by_title(key);
  const pid_t pid = onion_payload_launch_elfldr(key, buf, size);
  free(buf);
  /* Only persist real pids; never write 0/1 (PID 1 would hit system init). */
  if (pid > 1)
    onion_payload_write_pid_file(pid_path, pid);
  else
    onion_payload_write_pid_file(pid_path, -1);
  if (local.always_succeed_after_launch)
    return true;
  /* 0 = launch accepted but pid unknown; >1 = full success; -1 = fail. */
  return pid >= 0;
}
