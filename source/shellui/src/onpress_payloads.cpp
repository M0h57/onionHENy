/* Copyright (C) 2025 OnionHEN / LightningMods — OnPress payloads / auto-start */
#include "onpress.hpp"
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

void *load_payload_thread(void *args);

static int payload_read_pid_file(const char *pbuf) {
  int f = open(pbuf, O_RDONLY);
  if (f < 0) {
    return -1;
  }
  char t[32];
  int r = (int)read(f, t, sizeof(t) - 1);
  close(f);
  if (r <= 0) {
    return -1;
  }
  t[r] = 0;
  return atoi(t);
}

/** Valid userland payload pid only — never 0/1 (legacy "unknown" sentinel). */
static int payload_validate_pid(int pid, const char *pbuf, const char *tid) {
  if (pid <= 1) {
    if (pid == 1) {
      shellui_log("Ignoring bogus payload PID 1 for %s (legacy fallback)",
                  tid ? tid : "?");
      if (pbuf) {
        unlink(pbuf);
      }
    }
    return -1;
  }
  char name[32];
  if (sceKernelGetProcessName && sceKernelGetProcessName(pid, name) < 0) {
    shellui_log("Stale payload PID file for %s, removing", tid ? tid : "?");
    if (pbuf) {
      unlink(pbuf);
    }
    return -1;
  }
  return pid;
}

/**
 * Kill fallback only: title-specific process name.
 * Do NOT match bare "payload.elf" — multiple homebrews share that name;
 * the launch path must record the real pid in the .PID file.
 */
static int payload_resolve_pid_by_name(const char *tid) {
  if (!tid || !tid[0]) {
    return -1;
  }
  char nbuf[64];
  snprintf(nbuf, sizeof(nbuf), "%s.elf", tid);
  int pid = (int)onion_find_pid(nbuf);
  if (pid <= 1) {
    /* Orbis COMMLEN=19: long basenames appear truncated in ki_comm. */
    if (strlen(nbuf) > 19) {
      char trunc[20];
      memcpy(trunc, nbuf, 19);
      trunc[19] = '\0';
      pid = (int)onion_find_pid(trunc);
    }
  }
  if (pid <= 1) {
    pid = (int)onion_find_pid(tid);
  }
  if (pid <= 1) {
    pid = (int)onion_find_pid_substr(tid);
  }
  return pid > 1 ? pid : -1;
}

static OnPressResult prefix_id_payload(OnPressContext &ctx) {
  /* Only dynamic entries id_payload_<n> — not the link id_payloads. */
  if (ctx.id.rfind("id_payload_", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (g_ui.payloads_list.empty()) {
    return OnPressResult::Handled;
  }
  for (auto entry : g_ui.payloads_list) {
    if (entry.id != ctx.id) {
      continue;
    }
    char pbuf[256];
    snprintf(pbuf, sizeof(pbuf), "/system_tmp/%s.PID", entry.tid.c_str());
    /* Prefer PID recorded at launch (/system_tmp/<key>.PID). */
    int pid = payload_validate_pid(payload_read_pid_file(pbuf), pbuf,
                                   entry.tid.c_str());
    /* Missing / legacy pid=1: title-specific name only (not payload.elf). */
    if (pid <= 1) {
      pid = payload_resolve_pid_by_name(entry.tid.c_str());
      if (pid > 1) {
        shellui_log("Resolved %s via process name → pid %d", entry.tid.c_str(),
                    pid);
      }
    }
    if (pid > 1 && atol(ctx.value.c_str()) == 0) {
      shellui_log("killing recorded/resolved pid: %d (%s)", pid,
                  entry.tid.c_str());
      IPC_Client::getInstance(false).ForceKillPID(pid);
      unlink(pbuf);
      notify("%s killed", entry.tid.c_str());
      break;
    } else if (pid <= 1 && atol(ctx.value.c_str()) == 1) {
      pthread_t thr;
      shellui_log("Payload %s not running", entry.tid.c_str());
      auto info = new PayloadEntry(entry);
      pthread_create(&thr, nullptr, load_payload_thread, (void *)info);
    }
  }
  return OnPressResult::Handled;
}

static OnPressResult prefix_id_auto_payload(OnPressContext &ctx) {
  /* Only dynamic entries id_auto_payload_<n> — not a bare list title. */
  if (ctx.id.rfind("id_auto_payload_", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (g_ui.auto_payloads_list.empty()) {
    return OnPressResult::Handled;
  }
  for (auto entry : g_ui.auto_payloads_list) {
    if (entry.id != ctx.id) {
      continue;
    }
    std::string auto_path = entry.shellui_path + ".auto_start";
    shellui_log("Auto start path: %s", auto_path.c_str());
    if (if_exists(auto_path.c_str()) && !atol(ctx.value.c_str())) {
      unlink(auto_path.c_str());
    } else if (atol(ctx.value.c_str())) {
      int fd = open(auto_path.c_str(), O_CREAT | O_RDWR, 0777);
      if (fd < 0) {
        notify("Failed to create auto start file");
      } else {
        close(fd);
      }
    }
  }
  return OnPressResult::Handled;
}

static const OnPressPrefixEntry kPrefix[] = {
    {"id_auto_payload_", prefix_id_auto_payload},
    {"id_payload_", prefix_id_payload},
};

const OnPressPrefixEntry *onpress_payloads_prefix(size_t *count) {
  *count = sizeof(kPrefix) / sizeof(kPrefix[0]);
  return kPrefix;
}
