/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress payloads / auto-start */
#include "onpress.hpp"
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

void *load_payload_thread(void *args);

static OnPressResult prefix_id_payload(OnPressContext &ctx) {
  if (ctx.id.rfind("id_auto_payload", 0) == 0) {
    return OnPressResult::NotMine;
  }
  if (ctx.id.rfind("id_payload", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (g_ui.payloads_list.empty()) {
    return OnPressResult::Handled;
  }
  for (auto entry : g_ui.payloads_list) {
    if (entry.id != ctx.id) {
      continue;
    }
    int pid = -1;
    char pbuf[256];
    snprintf(pbuf, sizeof(pbuf), "/system_tmp/%s.PID", entry.tid.c_str());
    int f = open(pbuf, O_RDONLY);
    if (f >= 0) {
      char t[32];
      int r = read(f, t, sizeof(t) - 1);
      close(f);
      if (r > 0) {
        t[r] = 0;
        pid = atoi(t);
      }
    }
    if (pid > 0) {
      char name[32];
      if (sceKernelGetProcessName(pid, name) < 0) {
        shellui_log("Stale payload PID file for %s, removing", entry.tid.c_str());
        unlink(pbuf);
        pid = -1;
      }
    }
    if (pid > 0 && atol(ctx.value.c_str()) == 0) {
      shellui_log("killing pid: 0x%X", pid);
      IPC_Client::getInstance(false).ForceKillPID(pid);
      unlink(pbuf);
      notify("%s killed", entry.tid.c_str());
      break;
    } else if (pid <= 0 && atol(ctx.value.c_str()) == 1) {
      pthread_t thr;
      shellui_log("Payload %s not running", entry.tid.c_str());
      auto info = new PayloadEntry(entry);
      pthread_create(&thr, nullptr, load_payload_thread, (void *)info);
    }
  }
  return OnPressResult::Handled;
}

static OnPressResult prefix_id_auto_payload(OnPressContext &ctx) {
  if (ctx.id.rfind("id_auto_payload", 0) != 0) {
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
    {"id_auto_payload", prefix_id_auto_payload},
    {"id_payload", prefix_id_payload},
};

const OnPressPrefixEntry *onpress_payloads_prefix(size_t *count) {
  *count = sizeof(kPrefix) / sizeof(kPrefix[0]);
  return kPrefix;
}
