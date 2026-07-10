#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "util_platform.h"

#ifndef ORION_MAX_CHEATS
#define ORION_MAX_CHEATS 128
#endif
#ifndef ORION_MAX_PATCHES
#define ORION_MAX_PATCHES 128
#endif
#ifndef ORION_MAX_PATCH_BYTES
#define ORION_MAX_PATCH_BYTES 1024
#endif

/* Section bounds (matches util_module_info_t / NineS util_module_info_t) */
#ifndef MODULE_INFO_MAX_SECTIONS
#define MODULE_INFO_MAX_SECTIONS UTIL_MODULE_INFO_MAX_SECTIONS
#endif

typedef struct {
  bool code_cave_reloc;
  bool absolute;
  bool is_asm; /* ShnExt may leave unassembled ASM text */
  int section;
  uint64_t offset;
  size_t on_len;
  size_t off_len;
  uint8_t on[ORION_MAX_PATCH_BYTES];
  uint8_t off[ORION_MAX_PATCH_BYTES];
} orion_patch_t;

typedef struct {
  char name[128];
  char description[256];
  char module_name[128];
  bool enabled;
  size_t patch_count;
  size_t patch_capacity;
  orion_patch_t *patches;
} orion_cheat_entry_t;

typedef struct {
  char name[128];
  char process[128];
  size_t cheat_count;
  size_t cheat_capacity;
  int master_code_id;
  pid_t last_applied_pid;
  orion_cheat_entry_t *cheats;
} orion_cheat_file_t;

void orion_cheat_file_clear(orion_cheat_file_t *f);
int orion_cheat_file_ensure_cheat(orion_cheat_file_t *f);
int orion_cheat_entry_ensure_patch(orion_cheat_entry_t *e);

int orion_load_cheat_file(const char *path, orion_cheat_file_t *out);

int orion_cheat_load_buffer(const char *format, const unsigned char *data,
                            size_t data_len, orion_cheat_file_t *out);

int orion_toggle_cheat(const game_context_t *game, orion_cheat_file_t *file,
                       int cheat_index, char *status_out,
                       size_t status_out_size);

void orion_cheat_reset_state(orion_cheat_file_t *file);
