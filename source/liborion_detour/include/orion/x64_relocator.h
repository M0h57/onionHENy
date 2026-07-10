/* Copyright (C) 2026 OrionHEN
 *
 * Allocation-free x86-64 trampoline relocator. The caller owns the output
 * buffer and supplies the virtual source/destination execution addresses.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum orion_x64_relocate_error {
  ORION_X64_RELOCATE_OK = 0,
  ORION_X64_RELOCATE_INVALID_ARGUMENT,
  ORION_X64_RELOCATE_DECODE_ERROR,
  ORION_X64_RELOCATE_TOO_MANY_INSTRUCTIONS,
  ORION_X64_RELOCATE_OUTPUT_TOO_SMALL,
  ORION_X64_RELOCATE_UNSUPPORTED_RELATIVE,
  ORION_X64_RELOCATE_INTERNAL_TARGET_NOT_BOUNDARY,
  ORION_X64_RELOCATE_RIP_DISPLACEMENT_OUT_OF_RANGE,
};

struct orion_x64_relocate_result {
  size_t source_size;
  size_t trampoline_size;
  size_t error_offset;
  enum orion_x64_relocate_error error;
};

/* Worst-case output when stealing the first 14 bytes of a function. */
#define ORION_X64_TRAMPOLINE_CAPACITY 256u

/* Relocate whole instructions and append an absolute jump back to source. */
bool orion_x64_relocate(const uint8_t *source, uintptr_t source_address,
                        uint8_t *destination, uintptr_t destination_address,
                        size_t min_source_size, size_t destination_capacity,
                        struct orion_x64_relocate_result *result);

const char *orion_x64_relocate_error_string(
    enum orion_x64_relocate_error error);

#ifdef __cplusplus
}
#endif
