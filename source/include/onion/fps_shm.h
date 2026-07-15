#pragma once

/**
 * Shared FPS sample block (game probe / ShellUI scrape → overlay).
 *
 * Writers (either or both):
 *   - fps_elf in the game process — GNM flip count → pwrite samples
 *   - shellui — notification scrape → pwrite (classic Onion path)
 * Reader: shellui overlay — displays rolling mean FPS from SHM only.
 *
 * Game sandboxes usually cannot mkdir/create under /data or /system_tmp.
 * Privileged code (daemon / shellui) must call onion_fps_shm_ensure() first
 * so the file exists with mode 0666; fps_elf only opens RDWR.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ONION_FPS_SHM_MAGIC   0x4F465053u /* 'OFPS' */
#define ONION_FPS_SHM_VERSION 1u

/* PHU prefers system_tmp for cross-process probe I/O. */
#define ONION_FPS_SHM_PATH_TMP  "/system_tmp/onion_fps_shm"
#define ONION_FPS_SHM_PATH_DATA "/data/OnionHEN/fps_shm"
#define ONION_FPS_SHM_PATH_USER "/user/data/OnionHEN/fps_shm"

typedef struct onion_fps_shm {
  uint32_t magic;
  uint32_t version;
  float fps;
  float fps_vsync;
  uint64_t flip_total;
  uint64_t sample_ns;
  uint32_t hooks_armed;
  uint32_t flags; /* bit0: writer alive */
  char api_name[32];
} onion_fps_shm_t;

/** Paths tried in order (privileged create + game open). */
static inline const char *const *onion_fps_shm_paths(int *count) {
  static const char *const kPaths[] = {
      ONION_FPS_SHM_PATH_TMP,
      ONION_FPS_SHM_PATH_DATA,
      ONION_FPS_SHM_PATH_USER,
  };
  if (count)
    *count = (int)(sizeof(kPaths) / sizeof(kPaths[0]));
  return kPaths;
}

/**
 * Privileged: create zeroed sample files (0666) so the sandboxed game can
 * open them. Safe to call repeatedly. Returns 0 if at least one path is ready.
 */
static inline int onion_fps_shm_ensure(void) {
  int n = 0;
  const char *const *paths = onion_fps_shm_paths(&n);
  int ok = 0;
  onion_fps_shm_t zero;
  memset(&zero, 0, sizeof(zero));
  zero.magic = ONION_FPS_SHM_MAGIC;
  zero.version = ONION_FPS_SHM_VERSION;

  (void)mkdir("/data/OnionHEN", 0777);
  (void)mkdir("/user/data/OnionHEN", 0777);
  (void)mkdir("/system_tmp", 0777);

  for (int i = 0; i < n; ++i) {
    int fd = open(paths[i], O_RDWR | O_CREAT, 0666);
    if (fd < 0)
      continue;
    (void)fchmod(fd, 0666);
    if (ftruncate(fd, (off_t)sizeof(onion_fps_shm_t)) == 0) {
      /* Only seed magic if empty / invalid so we do not clobber live samples. */
      onion_fps_shm_t cur;
      ssize_t r = pread(fd, &cur, sizeof(cur), 0);
      if (r != (ssize_t)sizeof(cur) || cur.magic != ONION_FPS_SHM_MAGIC)
        (void)pwrite(fd, &zero, sizeof(zero), 0);
      ok = 1;
    }
    close(fd);
  }
  return ok ? 0 : -1;
}

/**
 * Sandboxed writer: open an existing sample file RDWR (no create).
 * Returns fd or -1. Logs via optional errbuf.
 */
static inline int onion_fps_shm_open_existing(char *errbuf, size_t errlen) {
  int n = 0;
  const char *const *paths = onion_fps_shm_paths(&n);
  for (int i = 0; i < n; ++i) {
    int fd = open(paths[i], O_RDWR);
    if (fd >= 0)
      return fd;
    if (errbuf && errlen) {
      snprintf(errbuf, errlen, "open(%s) errno=%d", paths[i], errno);
    }
  }
  return -1;
}

#ifdef __cplusplus
}
#endif
