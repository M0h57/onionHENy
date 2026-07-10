/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Compatibility shim — implementation lives in liborion_detour.
 * Historical Detour.h also pulled C APIs used by prx.cpp.
 */
#pragma once

#include <orion/Detour.h>

extern "C" {
#include "ucred.h"
#include "defs.h"
#include "../lib/libmprotect.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "ps5/mdbg.h"
}
