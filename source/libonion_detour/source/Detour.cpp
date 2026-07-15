/* Copyright (C) 2025 OnionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <onion/Detour.h>

#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <onion/ipc_client.hpp>
#include <onion/x64_relocator.h>

#include <machine/param.h>
#include <sys/mman.h>
#include <ps5/kernel.h>

/*
 * Host (shellui / fps_elf) exports sceKernelMprotect as a *function pointer*
 * filled by dlsym — not as a real code symbol. Declaring it as
 *   extern "C" int sceKernelMprotect(...);
 * makes the linker resolve CALL to the data object (.bss). Executing the
 * pointer bytes as instructions → SIGILL right after "Hooking …".
 *
 * be62388 Detour lived in shellui and called through that pointer via
 * external_symbols.hpp; keep the same shape here.
 */
extern int (*sceKernelMprotect)(void *addr, size_t len, int prot)
    __attribute__((weak));

// Optional: host sets true when userland mprotect works (kstuff HV path).
extern bool has_hv_bypass __attribute__((weak));

namespace {

constexpr int kProtRwx = PROT_EXEC | PROT_READ | PROT_WRITE;
constexpr size_t kMaxX64InstructionLength = 15;
/*
 * hde64_disasm may inspect a complete 15-byte instruction when the final
 * instruction starts at HOOK_LENGTH - 1. PS5 xotext is execute-only, so every
 * page in that decoder window must be made readable before relocation starts.
 */
constexpr size_t kRelocationReadLength =
    HOOK_LENGTH + kMaxX64InstructionLength - 1;
constexpr uintptr_t kNearAllocationStep = 64u * 1024u * 1024u;
constexpr unsigned kNearAllocationSteps = 31; /* 1984 MiB < INT32_MAX */

uintptr_t page_align_down(uintptr_t addr) {
  return addr & ~static_cast<uintptr_t>(PAGE_MASK);
}

int mprotect_user(void *addr, size_t len) {
  if (sceKernelMprotect == nullptr) {
    return -1;
  }
  return sceKernelMprotect(addr, len, kProtRwx);
}

int mprotect_rwx(void *addr, size_t len) {
  if (!addr || len == 0) {
    return -1;
  }
  /* Same policy as be62388 shellui Detour, plus kernel fallback on failure. */
  const bool hv = (&has_hv_bypass != nullptr) && has_hv_bypass;
  if (hv) {
    if (mprotect_user(addr, len) >= 0) {
      return 0;
    }
    /* Probe succeeded earlier; still fall back if this page rejects userland. */
  } else if (mprotect_user(addr, len) >= 0) {
    return 0;
  }
  return kernel_mprotect(getpid(), reinterpret_cast<uint64_t>(addr), len,
                         kProtRwx);
}

/** Make the page(s) covering [addr, addr+len) RWX. */
int mprotect_range_rwx(uintptr_t addr, size_t len) {
  const uintptr_t start = page_align_down(addr);
  const uintptr_t end = page_align_down(addr + len - 1) + PAGE_SIZE;
  return mprotect_rwx(reinterpret_cast<void *>(start),
                      static_cast<size_t>(end - start));
}

/**
 * Allocate executable stub memory. Do NOT use malloc — heap is not a reliable
 * RX region even after mprotect; jumping there causes SIGILL/SIGSEGV.
 */
void *map_exec_stub(uintptr_t address, size_t len, int extra_flags) {
  void *p = mmap(reinterpret_cast<void *>(address), len, kProtRwx,
                 MAP_PRIVATE | MAP_ANONYMOUS | extra_flags, -1, 0);
  if (p == MAP_FAILED) {
    return nullptr;
  }
  /* Some firmwares ignore PROT_EXEC on mmap — force with mprotect path. */
  if (mprotect_rwx(p, len) < 0) {
    munmap(p, len);
    return nullptr;
  }
  return p;
}

void *alloc_exec_stub(size_t len, uintptr_t near_address) {
  if (len == 0) {
    return nullptr;
  }

  /*
   * RIP-relative memory operands still use a signed disp32 after relocation.
   * FreeBSD's MAP_EXCL makes MAP_FIXED collision-safe: probe nearby pages
   * without replacing any existing ShellUI/system mapping.
   */
  const uintptr_t base = page_align_down(near_address);
  for (unsigned step = 1; step <= kNearAllocationSteps; ++step) {
    const uintptr_t distance =
        static_cast<uintptr_t>(step) * kNearAllocationStep;
    if (base <= UINTPTR_MAX - distance) {
      if (void *p = map_exec_stub(base + distance, len,
                                  MAP_FIXED | MAP_EXCL)) {
        return p;
      }
    }
    if (base >= distance) {
      if (void *p = map_exec_stub(base - distance, len,
                                  MAP_FIXED | MAP_EXCL)) {
        return p;
      }
    }
  }

  /* Non-RIP-relative prologues remain relocatable even if no near gap exists. */
  return map_exec_stub(0, len, 0);
}

} // namespace

void WriteJump(uint64_t address, uint64_t destination) {
  *reinterpret_cast<uint8_t *>(address) = 0xFF;
  *reinterpret_cast<uint8_t *>(address + 1) = 0x25;
  *reinterpret_cast<uint8_t *>(address + 2) = 0x00;
  *reinterpret_cast<uint8_t *>(address + 3) = 0x00;
  *reinterpret_cast<uint8_t *>(address + 4) = 0x00;
  *reinterpret_cast<uint8_t *>(address + 5) = 0x00;
  *reinterpret_cast<uint64_t *>(address + 6) = destination;
}

void ReadMemory(uint64_t address, void *buffer, int length) {
  memcpy(buffer, reinterpret_cast<void *>(address), length);
}

void WriteMemory(uint64_t address, void *buffer, int length) {
  memcpy(reinterpret_cast<void *>(address), buffer, length);
}

void PatchInJump(uint64_t address, void *destination) {
  if (!address || !destination) {
    return;
  }
  WriteJump(address, reinterpret_cast<uint64_t>(destination));
}

void *DetourFunction(uint64_t address, void *destination) {
  if (!address || !destination) {
    return nullptr;
  }

  shellui_log("Hooking %#02lx => %p", address, destination);

  const size_t stubLength = ONION_X64_TRAMPOLINE_CAPACITY;
  void *executableAddress = alloc_exec_stub(stubLength, address);
  if (!executableAddress) {
    shellui_log("DetourFunction: failed to allocate executable stub");
    return nullptr;
  }

  /* PS5 xotext is execute-only. Never decode/copy it before this succeeds. */
  if (mprotect_range_rwx(address, kRelocationReadLength) < 0) {
    shellui_log("DetourFunction: failed to mprotect target decoder window");
    munmap(executableAddress, stubLength);
    return nullptr;
  }

  onion_x64_relocate_result relocation{};
  if (!onion_x64_relocate(
          reinterpret_cast<const uint8_t *>(address), address,
          reinterpret_cast<uint8_t *>(executableAddress),
          reinterpret_cast<uintptr_t>(executableAddress), HOOK_LENGTH,
          stubLength, &relocation)) {
    shellui_log("DetourFunction: relocation failed at +%zu: %s",
                relocation.error_offset,
                onion_x64_relocate_error_string(relocation.error));
    munmap(executableAddress, stubLength);
    return nullptr;
  }

  PatchInJump(address, destination);

  shellui_log(
      "DetourFunction: target=%#02lx hook=%p trampoline=%p stolen=%zu emitted=%zu",
      address, destination, executableAddress, relocation.source_size,
      relocation.trampoline_size);
  return executableAddress;
}
