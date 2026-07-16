/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * fps_elf — inject into game process (no process suspend from daemon).
 *
 * Classic Onion counting: detour GNM submit/flip → rolling FPS.
 * Publish via file SHM only (no notification spam).
 * Hooks are deferred so the title can finish loading first.
 */

/* Do not include defs.h — it declares sceKernelMprotect as a function while
 * external_symbols.hpp / Detour expect a filled function *pointer*. */
#include "detour.h"
#include "external_symbols.hpp"
#include <ps5/klog.h>

#include <onion/fps_shm.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define u32 uint32_t
#define s32 int32_t

extern "C" {
long ptr_syscall = 0;

int sceKernelLoadStartModule(const char *moduleFileName, int args,
                             const void *argp, int flags, void *opt, int *pRes);
int sceKernelDlsym(int handle, const char *symbol, void **addrp);

s32 sceGnmSubmitAndFlipCommandBuffersForWorkload(
    u32 workload, u32 count, u32 *dcb_gpu_addrs[], u32 *dcb_sizes_in_bytes,
    u32 *ccb_gpu_addrs[], u32 *ccb_sizes_in_bytes, u32 vo_handle, u32 buf_idx,
    u32 flip_mode, u32 flip_arg);
}

void __syscall() {
  asm(".intel_syntax noprefix\n"
      "  mov rax, rdi\n"
      "  mov rdi, rsi\n"
      "  mov rsi, rdx\n"
      "  mov rdx, rcx\n"
      "  mov r10, r8\n"
      "  mov r8,  r9\n"
      "  mov r9,  qword ptr [rsp + 8]\n"
      "  call qword ptr [rip + ptr_syscall]\n"
      "  ret\n");
}

namespace {

std::atomic<uint64_t> g_flip_total{0};
std::atomic<uint32_t> g_hooks_armed{0};

int g_shm_fd = -1;
onion_fps_shm_t g_shm_cache{};

/* ---- resolve libkernel mprotect pointer (required by Detour) ---- */

bool resolve_kernel_syms() {
  void *mp = nullptr;
  if (sceKernelDlsym(0x2001, "sceKernelMprotect", &mp) == 0 && mp) {
    sceKernelMprotect = reinterpret_cast<int (*)(void *, size_t, int)>(mp);
    klog_printf("[fps] sceKernelMprotect resolved %p\n", mp);
  } else {
    klog_puts("[fps] sceKernelDlsym mprotect failed — will use kernel_mprotect");
  }
  return true;
}

bool wait_mprotect_ready() {
  char buff[256];
  for (int i = 0; i < 120; ++i) {
    if (sceKernelMprotect) {
      if (sceKernelMprotect(buff, sizeof(buff),
                            PROT_READ | PROT_WRITE | PROT_EXEC) == 0)
        return true;
    } else {
      return true;
    }
    klog_puts("[fps] waiting for mprotect...");
    sleep(1);
  }
  return false;
}

/* ---- single flip hook (classic Onion path) ---- */

using GnmFlipWorkload_t = s32 (*)(u32, u32, u32 **, u32 *, u32 **, u32 *, u32,
                                  u32, u32, u32);
static GnmFlipWorkload_t g_gnm_wl_orig = nullptr;

static s32 gnm_wl_hook(u32 a, u32 b, u32 **c, u32 *d, u32 **e, u32 *f, u32 g,
                       u32 h, u32 i, u32 j) {
  g_flip_total.fetch_add(1, std::memory_order_relaxed);
  return g_gnm_wl_orig(a, b, c, d, e, f, g, h, i, j);
}

bool install_gnm_hook() {
  void *linked = reinterpret_cast<void *>(
      &sceGnmSubmitAndFlipCommandBuffersForWorkload);
  if (!linked) {
    klog_puts("[fps] GNM-WL-link symbol missing");
    return false;
  }
  if (!InstallDetour(reinterpret_cast<uint64_t>(linked),
                     (void *)&gnm_wl_hook,
                     reinterpret_cast<void **>(&g_gnm_wl_orig))) {
    klog_printf("[fps] detour FAIL GNM-WL-link @%p\n", linked);
    return false;
  }
  g_hooks_armed.store(1, std::memory_order_relaxed);
  klog_printf("[fps] detour OK GNM-WL-link @%p tramp=%p\n", linked,
              reinterpret_cast<void *>(g_gnm_wl_orig));
  return true;
}

/* ---- SHM publish ---- */

bool open_shm() {
  char err[128] = {};
  int fd = onion_fps_shm_open_existing(err, sizeof(err));
  if (fd < 0) {
    klog_printf("[fps] shm open existing failed (%s)\n", err);
    if (onion_fps_shm_ensure() == 0)
      fd = onion_fps_shm_open_existing(err, sizeof(err));
  }
  if (fd < 0) {
    klog_printf("[fps] shm still unavailable (%s)\n", err);
    return false;
  }

  g_shm_fd = fd;
  std::memset(&g_shm_cache, 0, sizeof(g_shm_cache));
  g_shm_cache.magic = ONION_FPS_SHM_MAGIC;
  g_shm_cache.version = ONION_FPS_SHM_VERSION;
  g_shm_cache.flags = 1;
  g_shm_cache.hooks_armed = 0;
  std::snprintf(g_shm_cache.api_name, sizeof(g_shm_cache.api_name), "%s",
                "GNM-WL");
  pwrite(g_shm_fd, &g_shm_cache, sizeof(g_shm_cache), 0);
  klog_printf("[fps] shm writer ready fd=%d\n", g_shm_fd);
  return true;
}

void publish_once(uint64_t flips_delta, double dt) {
  float fps = (dt > 1e-6) ? static_cast<float>(flips_delta / dt) : 0.f;

  g_shm_cache.magic = ONION_FPS_SHM_MAGIC;
  g_shm_cache.version = ONION_FPS_SHM_VERSION;
  g_shm_cache.fps = fps;
  g_shm_cache.flip_total = g_flip_total.load(std::memory_order_relaxed);
  g_shm_cache.hooks_armed = g_hooks_armed.load(std::memory_order_relaxed);
  g_shm_cache.flags = 1;
  std::snprintf(g_shm_cache.api_name, sizeof(g_shm_cache.api_name), "%s",
                "GNM-WL");

  if (g_shm_fd >= 0)
    pwrite(g_shm_fd, &g_shm_cache, sizeof(g_shm_cache), 0);
}

void *publish_thread(void *) {
  uint64_t prev = g_flip_total.load(std::memory_order_relaxed);
  auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    usleep(500 * 1000); /* 2 Hz */
    auto t1 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(t1 - t0).count();
    uint64_t cur = g_flip_total.load(std::memory_order_relaxed);
    uint64_t d = cur - prev;
    prev = cur;
    t0 = t1;
    publish_once(d, dt);
  }
  return nullptr;
}

/*
 * Never patch GNM during early boot — that freezes many titles.
 * Daemon no longer suspends the process, so this sleep advances immediately.
 *
 * KNOWN ISSUE (see docs/fps-overlay-known-issues.md):
 * Even after a 20s defer, DetourFunction on the linked
 * sceGnmSubmitAndFlipCommandBuffersForWorkload import (InstructionSize=17)
 * freezes the title as soon as the hook arms. Keep arming OFF until a safe
 * flip surface / prologue strategy is verified.
 */
constexpr int kHookDeferSeconds = 20;

/* 0 = ship-safe (SHM only, no detour). 1 = experimental GNM arm. */
#ifndef ONION_FPS_ARM_GNM_HOOK
#define ONION_FPS_ARM_GNM_HOOK 0
#endif

void *hook_install_thread(void *) {
#if !ONION_FPS_ARM_GNM_HOOK
  klog_puts("[fps] GNM detour DISABLED (ONION_FPS_ARM_GNM_HOOK=0) — "
            "see docs/fps-overlay-known-issues.md");
  if (g_shm_fd >= 0) {
    std::snprintf(g_shm_cache.api_name, sizeof(g_shm_cache.api_name), "%s",
                  "disabled");
    g_shm_cache.hooks_armed = 0;
    pwrite(g_shm_fd, &g_shm_cache, sizeof(g_shm_cache), 0);
  }
  return nullptr;
#else
  klog_printf("[fps] deferring GNM hook for %ds (game must finish loading)\n",
              kHookDeferSeconds);
  sleep(kHookDeferSeconds);

  if (!wait_mprotect_ready())
    klog_puts("[fps] mprotect not ready — attempting hook anyway");

  if (!install_gnm_hook())
    klog_puts("[fps] FATAL: GNM hook failed");
  else
    klog_puts("[fps] GNM hook armed — publishing via SHM");

  if (g_shm_fd >= 0) {
    g_shm_cache.hooks_armed = g_hooks_armed.load(std::memory_order_relaxed);
    pwrite(g_shm_fd, &g_shm_cache, sizeof(g_shm_cache), 0);
  }
  return nullptr;
#endif
}

} // namespace

int main(int argc, char const *argv[]) {
  (void)argc;
  (void)argv;
  klog_puts("============== fps_elf (Onion GNM count + SHM) ==============");

  resolve_kernel_syms();

  if (!open_shm())
    klog_puts("[fps] FATAL: SHM open failed — overlay cannot read FPS");

  pthread_t pub;
  if (pthread_create(&pub, nullptr, publish_thread, nullptr) == 0)
    pthread_detach(pub);
  else
    klog_puts("[fps] publish thread create failed");

  pthread_t hooks;
  if (pthread_create(&hooks, nullptr, hook_install_thread, nullptr) == 0)
    pthread_detach(hooks);
  else {
    klog_puts("[fps] hook thread create failed — installing inline after defer");
    sleep(kHookDeferSeconds);
    (void)wait_mprotect_ready();
    (void)install_gnm_hook();
  }

  klog_printf("[fps] payload online shm_fd=%d (hooks deferred %ds)\n", g_shm_fd,
              kHookDeferSeconds);

  for (;;)
    sleep(0x10000);
  return 0;
}
