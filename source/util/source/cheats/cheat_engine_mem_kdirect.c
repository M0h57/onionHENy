#include "cheats/cheat_engine_internal.h"

#include <errno.h>
#include <sys/mman.h>

#include <ps5/kernel.h>

#include "pt.h"
#include "util_platform.h"

void OrionHEN_log(const char *fmt, ...);
extern int pt_attach_proc(pid_t pid);
extern int pt_detach_proc(pid_t pid, int sig);

#define PROC_VMSPACE_OFFSET 0x200

// Helper functions for page table walk and CR3 reading
static int get_proc_cr3(pid_t pid, uint64_t *cr3, uint64_t *dmap_base) {
  uint64_t proc = kernel_get_proc(pid);
  if (proc == 0) {
    return -1;
  }
  uint64_t vmspace = 0;
  if (kernel_copyout(proc + PROC_VMSPACE_OFFSET, &vmspace, sizeof(vmspace)) < 0 || vmspace == 0) {
    return -1;
  }
  uint32_t fw_major = util_system_fw_major();
  uint32_t vmspace_pmap_offset = (fw_major >= 0x600) ? 0x2E8 : 0x2E0;
  uint64_t ptrs[2] = {0};
  if (kernel_copyout(vmspace + vmspace_pmap_offset + 32, ptrs, sizeof(ptrs)) < 0) {
    return -1;
  }
  if (cr3) *cr3 = ptrs[1];
  if (dmap_base) *dmap_base = ptrs[0] - ptrs[1];
  return 0;
}

static uint64_t virt2phys(uintptr_t addr, uint64_t dmap, uint64_t pml, uint64_t *phys_limit) {
  for (int i = 39; i >= 12; i -= 9) {
    uint64_t inner_pml = 0;
    uint64_t pte_addr = dmap + pml + ((addr & (0x1ffULL << i)) >> (i - 3));
    if (kernel_copyout(pte_addr, &inner_pml, sizeof(inner_pml)) < 0) {
      return -1;
    }
    if (!(inner_pml & 1)) { // not present
      return -1;
    }
    if ((inner_pml & 128) || i == 12) { // huge page or leaf page
      inner_pml &= (1ULL << 52) - (1ULL << i);
      inner_pml |= addr & ((1ULL << i) - 1ULL);
      if (phys_limit) {
        *phys_limit = (inner_pml | ((1ULL << i) - 1ULL)) + 1ULL;
      }
      return inner_pml;
    }
    inner_pml &= (1ULL << 52) - (1ULL << 12);
    pml = inner_pml;
  }
  return -1;
}

static int kdirect_read(int pid, uint64_t addr, void *buf, size_t len) {
  uint64_t cr3 = 0;
  uint64_t dmap = 0;
  if (get_proc_cr3(pid, &cr3, &dmap) < 0) {
    OrionHEN_log("[Cheat] kdirect_read failed to get CR3/dmap for pid=%d", pid);
    return -1;
  }

  uint8_t *p_dst = buf;
  uint64_t vaddr = addr;
  size_t sz = len;

  while (sz > 0) {
    uint64_t phys_end = 0;
    uint64_t phys = virt2phys(vaddr, dmap, cr3, &phys_end);
    if (phys == (uint64_t)-1) {
      OrionHEN_log("[Cheat] kdirect_read: virt2phys failed for pid=%d addr=0x%llx",
                       pid, (unsigned long long)vaddr);
      return -1;
    }
    size_t chk = phys_end - phys;
    if (sz < chk) {
      chk = sz;
    }
    if (kernel_copyout(dmap + phys, p_dst, chk) < 0) {
      OrionHEN_log("[Cheat] kdirect_read: kernel_copyout failed for pid=%d phys=0x%llx",
                       pid, (unsigned long long)(dmap + phys));
      return -1;
    }
    vaddr += chk;
    p_dst += chk;
    sz -= chk;
  }
  return 0;
}

static int kdirect_write(int pid, uint64_t addr, const void *buf, size_t len) {
  uint64_t cr3 = 0;
  uint64_t dmap = 0;
  if (get_proc_cr3(pid, &cr3, &dmap) < 0) {
    OrionHEN_log("[Cheat] kdirect_write failed to get CR3/dmap for pid=%d", pid);
    return -1;
  }

  const uint8_t *p_src = buf;
  uint64_t vaddr = addr;
  size_t sz = len;

  while (sz > 0) {
    uint64_t phys_end = 0;
    uint64_t phys = virt2phys(vaddr, dmap, cr3, &phys_end);
    if (phys == (uint64_t)-1) {
      OrionHEN_log("[Cheat] kdirect_write: virt2phys failed for pid=%d addr=0x%llx",
                       pid, (unsigned long long)vaddr);
      return -1;
    }
    size_t chk = phys_end - phys;
    if (sz < chk) {
      chk = sz;
    }
    if (kernel_copyin(p_src, dmap + phys, chk) < 0) {
      OrionHEN_log("[Cheat] kdirect_write: kernel_copyin failed for pid=%d phys=0x%llx",
                       pid, (unsigned long long)(dmap + phys));
      return -1;
    }
    vaddr += chk;
    p_src += chk;
    sz -= chk;
  }
  return 0;
}

static int kdirect_code_cave_map(int pid, uint64_t addr, size_t len) {
  uint64_t page_start = cheat_page_align_down(addr);
  uint64_t page_end = cheat_page_align_up(addr + len);
  size_t page_len = (size_t)(page_end - page_start);

  if (pt_attach_proc(pid) < 0) {
    OrionHEN_log("[Cheat] kdirect code cave ptrace attach failed pid=%d addr=0x%llx errno=%d",
                     pid, (unsigned long long)addr, errno);
    return -1;
  }

  intptr_t mapped = pt_mmap(pid, (intptr_t)page_start, page_len,
                                 PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (mapped != (intptr_t)page_start) {
    OrionHEN_log("[Cheat] kdirect code cave mmap failed addr=0x%llx page=0x%llx len=0x%zx ret=0x%llx errno=%d",
                     (unsigned long long)addr, (unsigned long long)page_start,
                     page_len, (unsigned long long)mapped, errno);
    pt_detach_proc(pid, 0);
    return -1;
  }

  int mprot_ret = kernel_mprotect(pid, page_start, page_len,
                                  PROT_READ | PROT_WRITE | PROT_EXEC);
  if (mprot_ret < 0) {
    OrionHEN_log("[Cheat] kdirect code cave mprotect failed page=0x%llx len=0x%zx ret=%d errno=%d",
                     (unsigned long long)page_start, page_len, mprot_ret, errno);
    pt_detach_proc(pid, 0);
    return -1;
  }

  pt_detach_proc(pid, 0);
  OrionHEN_log("[Cheat] kdirect code cave mapped page=0x%llx len=0x%zx",
                   (unsigned long long)page_start, page_len);
  return 0;
}

static const remote_mem_ops_t kdirect_ops = {
    .read = kdirect_read,
    .write = kdirect_write,
    .code_cave_map = kdirect_code_cave_map,
    .attach = NULL,
    .detach = NULL,
};

const remote_mem_ops_t *cheat_mem_kdirect_ops(void) {
  return &kdirect_ops;
}
