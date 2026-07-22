/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Jailbreak FIFO watcher — extracted from commands.cpp.
 *
 * Protocol (homebrew → daemon):
 *   1. App is foreground "big app" with a whitelisted Title ID.
 *   2. App writes JSON {"PID":"<pid>"} to
 *        /mnt/sandbox/<TID>_<000-050>/download0/<name>
 *      Accepted names (legacy homebrew still uses the first):
 *        - etahen_jailbreak   (original etaHEN clients)
 *        - onionhen_jailbreak (rebrand)
 *   3. This thread polls and patches ucred + fd root via ps5-payload-sdk
 *      kernel_* helpers (runtime KERNEL_ADDRESS_*; not libhijacker static FW
 *      tables which currently stop at 10.60 and break getHijacker on 11.x).
 */

#include "daemon_ops.hpp"
#include "globalconf.hpp"

#include <onion/proc_query.h>
#include <onion/platform.h>
#include <onion/settings.hpp>
#include <onion/app_jailbreak_policy.hpp>
#include "onion_cjson.hpp"

#include <atomic>
#include <iomanip>
#include <sstream>
#include <string>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

extern "C" {
#include <ps5/kernel.h>
}

namespace {

pthread_mutex_t jb_lock = PTHREAD_MUTEX_INITIALIZER;

/** Same authid etaHEN / Hijacker::jailbreak historically used. */
constexpr uint64_t kJbAuthId = 0x4801000000000013ull;

bool is_whitelisted_app(const std::string &tid) {
  return onion::app_jailbreak::is_whitelisted(tid);
}

int jb_read_uid(pid_t pid) {
  /* SDK path — does not need libhijacker allproc offsets. */
  if (kernel_get_proc(pid) == 0) {
    return -1;
  }
  return static_cast<int>(kernel_get_ucred_uid(pid));
}

uint64_t jb_read_authid(pid_t pid) {
  if (kernel_get_proc(pid) == 0) {
    return 0;
  }
  return kernel_get_ucred_authid(pid);
}

const char *jb_whitelist_reason(const std::string &tid) {
  return onion::app_jailbreak::whitelist_reason(tid);
}

/**
 * Full app jailbreak (uid0 + authid + caps + optional sandbox escape).
 * Uses ps5-payload-sdk kernel helpers so it tracks the FW symbols the HEN
 * already resolved into KERNEL_ADDRESS_* (works on 11.60 where Hijacker
 * static allproc tables are missing).
 */
bool jb_apply_privileges(pid_t pid, bool escape_sandbox) {
  static const uint8_t kFullCaps[16] = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  };

  const intptr_t kproc = kernel_get_proc(pid);
  if (kproc == 0) {
    OnionHEN_log("[JB] kernel_get_proc(%d)=0 (ALLPROC=0x%lx rootvnode=0x%lx)",
                 static_cast<int>(pid),
                 static_cast<unsigned long>(KERNEL_ADDRESS_ALLPROC),
                 static_cast<unsigned long>(KERNEL_ADDRESS_ROOTVNODE));
    return false;
  }

  if (kernel_set_ucred_uid(pid, 0) != 0) {
    OnionHEN_log("[JB] kernel_set_ucred_uid failed pid=%d", static_cast<int>(pid));
    return false;
  }
  (void)kernel_set_ucred_ruid(pid, 0);
  (void)kernel_set_ucred_svuid(pid, 0);
  (void)kernel_set_ucred_rgid(pid, 0);

  /* cr_ngroups — not exposed by SDK helpers; layout matches Hijacker (0x10). */
  const intptr_t ucred = kernel_get_proc_ucred(pid);
  if (ucred) {
    const uint32_t ngroups = 0;
    (void)kernel_copyin(&ngroups, ucred + 0x10, sizeof(ngroups));
  }

  if (kernel_set_ucred_authid(pid, kJbAuthId) != 0) {
    OnionHEN_log("[JB] kernel_set_ucred_authid failed pid=%d",
                 static_cast<int>(pid));
    return false;
  }
  if (kernel_set_ucred_caps(pid, kFullCaps) != 0) {
    OnionHEN_log("[JB] kernel_set_ucred_caps failed pid=%d",
                 static_cast<int>(pid));
    return false;
  }

  /* Match Hijacker: low byte of sceAttr = 0x80. */
  const uint64_t attrs = (kernel_get_ucred_attrs(pid) & ~0xffull) | 0x80ull;
  (void)kernel_set_ucred_attrs(pid, attrs);

  if (escape_sandbox) {
    const intptr_t root = kernel_get_root_vnode();
    if (root == 0) {
      OnionHEN_log("[JB] kernel_get_root_vnode()=0 — cannot escape sandbox");
      return false;
    }
    if (kernel_set_proc_rootdir(pid, root) != 0) {
      OnionHEN_log("[JB] kernel_set_proc_rootdir failed pid=%d",
                   static_cast<int>(pid));
      return false;
    }
    /* Hijacker sets fd_jdir = rootvnode (not 0). */
    if (kernel_set_proc_jaildir(pid, root) != 0) {
      OnionHEN_log("[JB] kernel_set_proc_jaildir failed pid=%d",
                   static_cast<int>(pid));
      return false;
    }
  }

  return kernel_get_ucred_uid(pid) == 0;
}

} // namespace

void *fifo_and_dumper_thread(void *args) noexcept {
  (void)args;
  char *json_str = nullptr;
  std::string tid, sandbox_dir_base;
  bool fifo_found = false;

  constexpr useconds_t kIdleUsleep = 200 * 1000; /* 200ms when nothing to do */
  /* Only for early-spawn race (sysctl alive, kproc not linked yet). */
  constexpr int kProcResolveAttempts = 3;
  constexpr useconds_t kProcResolveUsleep = 30 * 1000;

  std::string last_tid_logged;
  std::string last_reject_logged;
  int no_app_log_suppress = 0;

  OnionHEN_log("[JB] fifo watcher started (whitelist: ITEM00001, NPXS39041, "
               "PKGI13337, PKGI12345, TOOL00001, *LAPY*)");
  OnionHEN_log("[JB] kernel symbols: ALLPROC=0x%lx ROOTVNODE=0x%lx "
               "(0 means SDK did not resolve — jailbreak will fail)",
               static_cast<unsigned long>(KERNEL_ADDRESS_ALLPROC),
               static_cast<unsigned long>(KERNEL_ADDRESS_ROOTVNODE));

  while (true) {
    std::string sandbox_dir;

    /* Stack teardown: do not relaunch util or keep JB/FPS work. */
    if (g_stack_shutting_down.load(std::memory_order_acquire)) {
      sleep(1);
      continue;
    }

    pthread_mutex_lock(&jb_lock);

    {
      const onion::Settings cfg = g_settings.snapshot();
      if (cfg.enable_fan_speed)
        set_fan_threshold(cfg.fan_threshold);
    }

    int bappid = 0;
    if (!Get_Running_App_TID(tid, bappid)) {
      /*
       * API fail = no running big-app, OR GetAppTitleId failed mid-transition
       * (suspend/exit/home). Not a jailbreak failure by itself.
       */
      if (!last_tid_logged.empty()) {
        OnionHEN_log("[JB] lost foreground big-app (was tid=%s) — "
                     "Get_Running_App_TID failed (quit/home/suspend/crash/"
                     "sceSystemServiceGetAppIdOfRunningBigApp<0)",
                     last_tid_logged.c_str());
        last_tid_logged.clear();
        last_reject_logged.clear();
      }
      pthread_mutex_unlock(&jb_lock);
      /* Avoid spinning at 100% when home has no big app. */
      if (++no_app_log_suppress >= 50) {
        /* ~10s at 200ms — rare heartbeat so we know watcher is alive. */
        OnionHEN_log("[JB] idle: still no foreground big-app "
                     "(Get_Running_App_TID fail; heartbeat)");
        no_app_log_suppress = 0;
      }
      usleep(kIdleUsleep);
      continue;
    }
    no_app_log_suppress = 0;

    if (tid != last_tid_logged) {
      OnionHEN_log("[JB] foreground big-app tid=%s appid=%d whitelist=%s",
                   tid.c_str(), bappid, jb_whitelist_reason(tid));
      last_tid_logged = tid;
      last_reject_logged.clear();
    }

    if (!is_whitelisted_app(tid)) {
      if (tid != last_reject_logged) {
        OnionHEN_log("[JB] skip tid=%s — not on jailbreak whitelist "
                     "(add TID or use LAPY* / known PKGI ids)",
                     tid.c_str());
        last_reject_logged = tid;
      }
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    sandbox_dir_base = "/mnt/sandbox/" + tid + "_";
    fifo_found = false;
    int sandbox_slot = -1;
    const char *req_name = nullptr;
    /* Legacy etaHEN clients still write etahen_jailbreak; accept both. */
    static const char *const kJailbreakReqNames[] = {
        "etahen_jailbreak",
        "onionhen_jailbreak",
    };

    for (int i = 0; i <= 50 && !fifo_found; ++i) {
      std::ostringstream oss;
      oss << std::setw(3) << std::setfill('0') << i;
      const std::string slot_prefix =
          sandbox_dir_base + oss.str() + "/download0/";
      for (const char *name : kJailbreakReqNames) {
        sandbox_dir = slot_prefix + name;
        if (if_exists(sandbox_dir.c_str())) {
          fifo_found = true;
          sandbox_slot = i;
          req_name = name;
          break;
        }
      }
    }

    if (!fifo_found) {
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    OnionHEN_log("[JB] request file found: %s (slot=%03d tid=%s name=%s)",
                 sandbox_dir.c_str(), sandbox_slot, tid.c_str(),
                 req_name ? req_name : "?");

    if (!GetFileContents(sandbox_dir.c_str(), &json_str)) {
      OnionHEN_log("[JB] FAIL: cannot read request file %s (errno path empty?)",
                   sandbox_dir.c_str());
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    OnionHEN_log("[JB] request body: %s", json_str ? json_str : "(null)");
    onion_cjson::Root my_json(json_str);
    if (!my_json) {
      OnionHEN_log("[JB] FAIL: JSON parse error for body: %s",
                   json_str ? json_str : "(null)");
      free(json_str);
      json_str = nullptr;
      /* Leave file for client retry; do not unlink on parse fail. */
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    const char *PID = onion_cjson::string_item(my_json.get(), "PID");
    if (!PID) {
      OnionHEN_log("[JB] FAIL: JSON missing string field \"PID\" "
                   "(expected {\"PID\":\"1234\"})");
      free(json_str);
      json_str = nullptr;
      unlink(sandbox_dir.c_str());
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    const int reserved_value = atoi(PID);
    const bool alive = isProcessAlive(reserved_value);
    OnionHEN_log("[JB] target pid=%d (from JSON PID=\"%s\") alive=%d "
                 "kproc=0x%lx",
                 reserved_value, PID, alive ? 1 : 0,
                 static_cast<unsigned long>(
                     reserved_value > 1 ? kernel_get_proc(reserved_value) : 0));

    if (reserved_value <= 1) {
      OnionHEN_log("[JB] FAIL: invalid pid=%d (must be > 1); clearing request",
                   reserved_value);
      free(json_str);
      json_str = nullptr;
      unlink(sandbox_dir.c_str());
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    if (!alive) {
      OnionHEN_log("[JB] FAIL: pid=%d is dead — stale request; clearing %s",
                   reserved_value, sandbox_dir.c_str());
      free(json_str);
      json_str = nullptr;
      unlink(sandbox_dir.c_str());
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    /*
     * Failure path: log only — never onion_notify (user asked not to spam
     * the UI on failed jailbreaks). Success may still notify when
     * app_jailbreak.debug_notifications is enabled.
     *
     * Retries: at most a few short attempts for early-spawn race. Do not
     * loop 30× — if ALLPROC is wrong/unresolved, more tries will never help.
     */
    if (KERNEL_ADDRESS_ALLPROC == 0) {
      OnionHEN_log("[JB] FAIL: KERNEL_ADDRESS_ALLPROC=0 (SDK symbols not "
                   "resolved for this FW); clearing request");
      free(json_str);
      json_str = nullptr;
      unlink(sandbox_dir.c_str());
      pthread_mutex_unlock(&jb_lock);
      usleep(kIdleUsleep);
      continue;
    }

    const int uid_before = jb_read_uid(reserved_value);
    const uint64_t auth_before = jb_read_authid(reserved_value);
    OnionHEN_log("[JB] pre-jb uid=%d authid=0x%llx", uid_before,
                 static_cast<unsigned long long>(auth_before));

    bool ok = false;
    for (int attempt = 1; attempt <= kProcResolveAttempts && !ok; ++attempt) {
      if (kernel_get_proc(reserved_value) == 0) {
        OnionHEN_log("[JB] kernel_get_proc(%d)=0 attempt=%d/%d",
                     reserved_value, attempt, kProcResolveAttempts);
        if (!isProcessAlive(reserved_value)) {
          break;
        }
        if (attempt < kProcResolveAttempts) {
          usleep(kProcResolveUsleep);
        }
        continue;
      }
      OnionHEN_log("[JB] applying privileges pid=%d (SDK kernel_*, "
                   "authid=0x%llx, escape_sandbox=1)",
                   reserved_value,
                   static_cast<unsigned long long>(kJbAuthId));
      ok = jb_apply_privileges(static_cast<pid_t>(reserved_value),
                               /*escape_sandbox=*/true);
      break; /* apply once we resolved kproc — no multi-retry on apply fail */
    }

    const int uid_after = jb_read_uid(reserved_value);
    const uint64_t auth_after = jb_read_authid(reserved_value);
    OnionHEN_log("[JB] post-jb uid=%d (was %d) authid=0x%llx (was 0x%llx) "
                 "ok=%d",
                 uid_after, uid_before,
                 static_cast<unsigned long long>(auth_after),
                 static_cast<unsigned long long>(auth_before), ok ? 1 : 0);

    if (ok && uid_after == 0) {
      /* Only success may surface a toast, and only when debug notify is on. */
      if (g_settings.snapshot().debug_app_jb_msg)
        onion_notify(true, "App (PID %i) has been granted a jailbreak",
                     reserved_value);
      OnionHEN_log("[JB] OK: pid=%d tid=%s fully jailbroken", reserved_value,
                   tid.c_str());
    } else {
      OnionHEN_log("[JB] FAIL: privilege apply did not stick for pid=%d "
                   "(uid=%d) — see pre/post logs; no notify",
                   reserved_value, uid_after);
    }

    /* Always clear request so we do not reprocess the same JSON. */
    if (unlink(sandbox_dir.c_str()) != 0) {
      OnionHEN_log("[JB] WARN: unlink request file failed path=%s",
                   sandbox_dir.c_str());
    } else {
      OnionHEN_log("[JB] cleared request file %s", sandbox_dir.c_str());
    }

    free(json_str);
    json_str = nullptr;
    pthread_mutex_unlock(&jb_lock);
    usleep(kIdleUsleep);
  }

  return nullptr;
}
