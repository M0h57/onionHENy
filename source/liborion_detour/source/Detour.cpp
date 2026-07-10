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
#include <ps5/kernel.h>

extern "C" int sceKernelMprotect(void *addr, size_t len, int prot);

// Optional: host may set this for HV-bypass environments (shellui).
// When true, prefer sceKernelMprotect without probing failure first.
extern bool has_hv_bypass __attribute__((weak));

static int mprotect_rwx(void *addr, size_t len) {
  const int prot = PROT_EXEC | PROT_READ | PROT_WRITE;
  const bool hv = (&has_hv_bypass != nullptr) && has_hv_bypass;
  if (hv) {
    return sceKernelMprotect(addr, len, prot);
  }
  if (sceKernelMprotect(addr, len, prot) >= 0) {
    return 0;
  }
  return kernel_mprotect(getpid(), reinterpret_cast<uint64_t>(addr), len, prot);
}

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

  if (mprotect_rwx(reinterpret_cast<void *>(address), PAGE_SIZE) < 0) {
    shellui_log("DetourFunction: failed to mprotect target page");
  }

  while (InstructionSize < HOOK_LENGTH) {
    hde64s hs{};
    uint32_t temp =
        hde64_disasm(reinterpret_cast<void *>(address + InstructionSize), &hs);
    if (hs.flags & F_ERROR) {
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

  int stubLength = static_cast<int>(InstructionSize + HOOK_LENGTH);
  void *executableAddress = malloc(stubLength);
  if (!executableAddress) {
    shellui_log("Failed to allocate memory for stub");
    return nullptr;
  }

  if (mprotect_rwx(executableAddress, stubLength) < 0) {
    shellui_log("DetourFunction: failed to mprotect stub");
  }

  ReadMemory(address, executableAddress, static_cast<int>(InstructionSize));
  PatchInJump(reinterpret_cast<uint64_t>(executableAddress) + InstructionSize,
              reinterpret_cast<void *>(address + InstructionSize));
  PatchInJump(address, destination);

  return executableAddress;
}
