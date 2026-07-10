/* Copyright (C) 2025 OrionHEN / LightningMods

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

#include <orion/Detour.h>

#include <cstdint>
#include <cstring>
#include <cstdlib>

#include <orion/hde64.h>
#include <orion/ipc_client.hpp>

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
void *alloc_exec_stub(size_t len) {
  if (len == 0) {
    return nullptr;
  }
  void *p = mmap(nullptr, len, kProtRwx, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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

  uint32_t InstructionSize = 0;
  shellui_log("Hooking %#02lx => %p", address, destination);

  if (mprotect_range_rwx(address, HOOK_LENGTH) < 0) {
    shellui_log("DetourFunction: failed to mprotect target page(s)");
    return nullptr;
  }

  while (InstructionSize < HOOK_LENGTH) {
    hde64s hs{};
    uint32_t temp =
        hde64_disasm(reinterpret_cast<void *>(address + InstructionSize), &hs);
    if (hs.flags & F_ERROR) {
      shellui_log("DetourFunction: disasm error at +%u", InstructionSize);
      return nullptr;
    }
    InstructionSize += temp;
  }

  shellui_log("InstructionSize: %i", InstructionSize);

  if (InstructionSize < HOOK_LENGTH) {
    shellui_log(
        "DetourFunction: Hooking Requires a minimum of 14 bytes to write jump!");
    return nullptr;
  }

  const size_t stubLength =
      static_cast<size_t>(InstructionSize) + static_cast<size_t>(HOOK_LENGTH);
  void *executableAddress = alloc_exec_stub(stubLength);
  if (!executableAddress) {
    shellui_log("DetourFunction: failed to allocate executable stub");
    return nullptr;
  }

  ReadMemory(address, executableAddress, static_cast<int>(InstructionSize));
  PatchInJump(reinterpret_cast<uint64_t>(executableAddress) + InstructionSize,
              reinterpret_cast<void *>(address + InstructionSize));
  PatchInJump(address, destination);

  shellui_log("DetourFunction: target=%#02lx hook=%p trampoline=%p size=%u",
              address, destination, executableAddress, InstructionSize);
  return executableAddress;
}
