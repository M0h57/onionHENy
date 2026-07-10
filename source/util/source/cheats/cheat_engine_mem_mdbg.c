#include "cheats/cheat_engine_internal.h"

#include <errno.h>
#include <sys/mman.h>

#include <ps5/kernel.h>
#include <ps5/mdbg.h>

#include "pt.h"

void OrionHEN_log(const char *fmt, ...);
extern int pt_attach_proc(pid_t pid);
extern int pt_detach_proc(pid_t pid, int sig);

static int mdbg_read(int pid, uint64_t addr, void *buf, size_t len) {
  int rc = mdbg_copyout(pid, (intptr_t)addr, buf, len);
  OrionHEN_log("[Cheat] mdbg_read: pid=%d addr=0x%llx len=%zu ret=%d errno=%d",
                   pid, (unsigned long long)addr, len, rc, errno);
  return rc;
}

static int mdbg_write(int pid, uint64_t addr, const void *buf, size_t len) {
  int rc = mdbg_copyin(pid, buf, (intptr_t)addr, len);
  OrionHEN_log("[Cheat] mdbg_write: pid=%d addr=0x%llx len=%zu ret=%d errno=%d",
                   pid, (unsigned long long)addr, len, rc, errno);
  return rc;
}

static int mdbg_code_cave_map(int pid, uint64_t addr, size_t len) {
  uint64_t page_start = cheat_page_align_down(addr);
  uint64_t page_end = cheat_page_align_up(addr + len);
  size_t page_len = (size_t)(page_end - page_start);

  if (pt_attach_proc(pid) < 0) {
    OrionHEN_log("[Cheat] mdbg code cave ptrace attach failed pid=%d "
                     "addr=0x%llx errno=%d",
                     pid, (unsigned long long)addr, errno);
    return -1;
  }

  intptr_t mapped = pt_mmap(pid, (intptr_t)page_start, page_len,
                                 PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (mapped != (intptr_t)page_start) {
    OrionHEN_log("[Cheat] mdbg code cave mmap failed addr=0x%llx "
                     "page=0x%llx len=0x%zx ret=0x%llx errno=%d",
                     (unsigned long long)addr, (unsigned long long)page_start,
                     page_len, (unsigned long long)mapped, errno);
    pt_detach_proc(pid, 0);
    return -1;
  }

  int mprot_ret = kernel_mprotect(pid, page_start, page_len,
                                  PROT_READ | PROT_WRITE | PROT_EXEC);
  if (mprot_ret < 0) {
    OrionHEN_log("[Cheat] mdbg code cave mprotect failed page=0x%llx "
                     "len=0x%zx ret=%d errno=%d",
                     (unsigned long long)page_start, page_len, mprot_ret, errno);
    pt_detach_proc(pid, 0);
    return -1;
  }

  pt_detach_proc(pid, 0);
  OrionHEN_log("[Cheat] mdbg code cave mapped page=0x%llx len=0x%zx",
                   (unsigned long long)page_start, page_len);
  return 0;
}

static const remote_mem_ops_t mdbg_ops = {
    .read = mdbg_read,
    .write = mdbg_write,
    .code_cave_map = mdbg_code_cave_map,
    .attach = NULL,
    .detach = NULL,
};

const remote_mem_ops_t *cheat_mem_mdbg_ops(void) {
  return &mdbg_ops;
}
