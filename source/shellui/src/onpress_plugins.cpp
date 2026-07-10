/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress plugins / auto-start */
#include "onpress.hpp"
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

void *load_plugin_thread(void *args);
int sceSystemServiceGetAppId(const char *tid);

static OnPressResult prefix_id_plugin(OnPressContext &ctx) {
  // matches id_plugin* but not id_auto_plugin*
  if (ctx.id.rfind("id_auto_plugin", 0) == 0) {
    return OnPressResult::NotMine;
  }
  if (ctx.id.rfind("id_plugin", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (g_ui.plugins_list.empty()) {
    return OnPressResult::Handled;
  }
  for (auto plugin : g_ui.plugins_list) {
    if (plugin.id != ctx.id) {
      continue;
    }
    int pid = -1;
    /* tid is plugin TitleID, or raw-ELF stem (no ".elf"); PID file is
     * /system_tmp/<tid>.PID written by orion_plugin_load. */
    char pbuf[256];
    snprintf(pbuf, sizeof(pbuf), "/system_tmp/%s.PID", plugin.tid.c_str());
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
        shellui_log("Stale plugin PID file detected for %s, removing",
                    plugin.tid.c_str());
        unlink(pbuf);
        pid = -1;
      }
    }
    if (pid > 0 && atol(ctx.value.c_str()) == 0) {
      shellui_log("killing pid: 0x%X", pid);
      IPC_Client::getInstance(false).ForceKillPID(pid);
      if (plugin.tid == "XMLS00001") {
        unlink("/system_tmp/patch_plugin");
      }
      unlink(pbuf);
      notify("%s killed", plugin.tid.c_str());
      break;
    } else if (pid <= 0 && atol(ctx.value.c_str()) == 1) {
      pthread_t thr;
      shellui_log("Plugin %s not running", plugin.tid.c_str());
      auto plugin_info = new Plugins(plugin);
      pthread_create(&thr, nullptr, load_plugin_thread, (void *)plugin_info);
    }
  }
  return OnPressResult::Handled;
}

static OnPressResult prefix_id_auto_plugin(OnPressContext &ctx) {
  if (ctx.id.rfind("id_auto_plugin", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (g_ui.auto_list.empty()) {
    return OnPressResult::Handled;
  }
  for (auto plugin : g_ui.auto_list) {
    if (plugin.id != ctx.id) {
      continue;
    }
    std::string auto_path = plugin.shellui_path + ".auto_start";
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
    {"id_auto_plugin", prefix_id_auto_plugin},
    {"id_plugin", prefix_id_plugin},
};

const OnPressPrefixEntry *onpress_plugins_prefix(size_t *count) {
  *count = sizeof(kPrefix) / sizeof(kPrefix[0]);
  return kPrefix;
}
