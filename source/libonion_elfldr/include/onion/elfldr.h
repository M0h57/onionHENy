/* Copyright (C) 2024 John Törnblom / OnionHEN
 *
 * Shared ELF load helpers (inject path + privilege raise).
 * Process spawn goes through external elfldr :9021 (elfldr_remote).
 */

#pragma once

#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Load ELF image into target process; returns entry VA or 0 on failure. */
intptr_t elfldr_load(pid_t pid, uint8_t *elf);

/** Allocate payload_args_t-compatible block in target. Returns VA or 0. */
intptr_t elfldr_payload_args(pid_t pid);

/**
 * Escape jail and raise privileges for pid.
 * Does not use ptrace — safe for self (getpid()).
 */
int elfldr_raise_privileges(pid_t pid);

#ifdef __cplusplus
}
#endif
