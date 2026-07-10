#include "cheats/cheat_engine_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pt.h"
#include "util_platform.h"

#include <unistd.h>

void OrionHEN_log(const char *fmt, ...);

const remote_mem_ops_t *remote_ops_for_fw(int fw_major) {
  return (fw_major >= 0x840) ? cheat_mem_kdirect_ops() : cheat_mem_mdbg_ops();
}

static int effective_fw_major(void) {
  return (int)util_system_fw_major();
}

static int patch_matches(const uint8_t *lhs, const uint8_t *rhs, size_t len) {
  return memcmp(lhs, rhs, len) == 0 ? 0 : -1;
}

static bool patch_current_is_expected(const orion_cheat_entry_t *entry,
                                      const orion_patch_t *patch,
                                      const uint8_t *current,
                                      size_t current_len) {
  const uint8_t *expected = entry->enabled ? patch->on : patch->off;
  size_t expected_len = entry->enabled ? patch->on_len : patch->off_len;
  const uint8_t *opposite = entry->enabled ? patch->off : patch->on;
  size_t opposite_len = entry->enabled ? patch->off_len : patch->on_len;

  if (current_len == expected_len &&
      patch_matches(current, expected, expected_len) == 0) {
    return true;
  }
  if (current_len == opposite_len &&
      patch_matches(current, opposite, opposite_len) == 0) {
    return true;
  }
  return false;
}

static void log_patch_bytes(const char *label, size_t patch_index,
                            const uint8_t *bytes, size_t len) {
  char hex[128] = {0};
  size_t shown = len > 16 ? 16 : len;

  for (size_t b = 0; b < shown; ++b) {
    sprintf(hex + (b * 3), "%02X ", bytes[b]);
  }
  OrionHEN_log("[Cheat] Patch %zu: %s: %s%s", patch_index, label, hex,
               len > 16 ? "..." : "");
}

static uint64_t resolve_patch_address(const util_module_info_t *module_info,
                                      const orion_patch_t *patch, bool is_ps2,
                                      uint64_t default_base,
                                      const game_context_t *game, int *ok) {
  uint64_t offset = patch->offset;
  uint64_t addr;

  *ok = 1;
  if (is_ps2 || patch->absolute) {
    return patch->offset;
  }

  /*
   * section is only a master-code dependency marker; base is always
   * sections[0].vaddr (default_base).
   */
  if (patch->section == 0) {
    offset = orion_cheat_normalize_ps4_eboot_offset(game, module_info, offset,
                                                    is_ps2);
  }
  addr = default_base + offset;
  (void)module_info;
  return addr;
}

static int fix_master_code_dependency(const game_context_t *game,
                                      orion_cheat_file_t *file,
                                      orion_cheat_entry_t *entry,
                                      uint64_t base_address,
                                      const util_module_info_t *module_info,
                                      bool is_ps2, int fw_major) {
  orion_cheat_entry_t *master = NULL;
  orion_patch_t *master_patch = NULL;
  orion_patch_t *dep_patch = NULL;
  uint8_t *patched_code = NULL;
  uint64_t mc_address = 0;

  if (file->master_code_id < 0 ||
      !orion_cheat_contains_token(entry->name, "MC") ||
      entry->patch_count != 1 || entry->patches[0].section == 0) {
    return 0;
  }

  master = &file->cheats[file->master_code_id];
  if (master->patch_count == 0) {
    return 0;
  }

  master_patch = &master->patches[0];
  dep_patch = &entry->patches[0];
  mc_address =
      master_patch->absolute
          ? master_patch->offset
          : (base_address + orion_cheat_normalize_ps4_eboot_offset(
                                game, module_info, master_patch->offset, is_ps2));
  patched_code = (uint8_t *)calloc(master_patch->on_len, 1);
  if (patched_code == NULL) {
    return -1;
  }

  {
    const remote_mem_ops_t *ops = remote_ops_for_fw(fw_major);
    if (ops->read(game->pid, mc_address, patched_code, master_patch->on_len) <
        0) {
      free(patched_code);
      return -1;
    }
  }

  for (size_t i = 0; i + dep_patch->off_len <= master_patch->on_len; ++i) {
    if (memcmp(patched_code + i, dep_patch->off, dep_patch->off_len) == 0) {
      dep_patch->offset = master_patch->offset + i;
      free(patched_code);
      return 0;
    }
  }

  dep_patch->offset =
      ((master_patch->offset >> 8) << 8) | (dep_patch->offset & 0xff);
  free(patched_code);
  return 0;
}

int orion_cheat_toggle_entry(const game_context_t *game, orion_cheat_file_t *file,
                             int cheat_index, char *status_out,
                             size_t status_out_size) {
  orion_cheat_entry_t *entry = NULL;
  util_module_info_t module_info;
  util_module_info_t ps2_module;
  game_context_t target_game;
  uint64_t base_address = 0;
  bool is_ps2 = false;
  bool enabled = false;
  int result = 0;
  int fw_major = 0;
  pid_t target_pid = -1;

  if (cheat_index < 0 || (size_t)cheat_index >= file->cheat_count) {
    snprintf(status_out, status_out_size, "invalid cheat index %d", cheat_index);
    return -1;
  }

  entry = &file->cheats[cheat_index];
  target_game = *game;
  target_pid = game->pid;
  fw_major = effective_fw_major();

  if (fw_major == 0) {
    OrionHEN_log("[Cheat] firmware version unavailable; refusing memory write");
    snprintf(status_out, status_out_size, "%s -> firmware version unavailable",
             entry->name);
    return -1;
  }

  const remote_mem_ops_t *ops = remote_ops_for_fw(fw_major);

  if (file->last_applied_pid != 0 && file->last_applied_pid != target_pid) {
    OrionHEN_log("[Cheat] PID changed %d -> %d, resetting cheat states",
                 (int)file->last_applied_pid, (int)target_pid);
    for (size_t j = 0; j < file->cheat_count; ++j) {
      file->cheats[j].enabled = false;
    }
    file->master_code_id = -1;
  }

  OrionHEN_log("[Cheat] Toggle '%s' (Module: %s, Patches: %zu, Current: %s)",
               entry->name, entry->module_name, entry->patch_count,
               entry->enabled ? "ON" : "OFF");

  if (entry->module_name[0] == '\0' && game->process_name[0] != '\0') {
    OrionHEN_log("[Cheat] module_name empty, using process_name '%s'",
                 game->process_name);
    snprintf(entry->module_name, sizeof(entry->module_name), "%s",
             game->process_name);
  }

  if (util_find_module(target_pid, entry->module_name, &module_info) < 0) {
    pid_t fallback_pid = -1;

    OrionHEN_log("[Cheat] Module '%s' not found for PID %d, scanning appid %d",
                 entry->module_name, target_pid, game->appid);
    if (util_find_module_in_app(game->appid, entry->module_name, &fallback_pid,
                                &module_info) < 0) {
      OrionHEN_log("[Cheat] Module '%s' not found for appid %d",
                   entry->module_name, game->appid);
      snprintf(status_out, status_out_size, "module not found: %s",
               entry->module_name);
      return -1;
    }

    target_pid = fallback_pid;
    target_game.pid = fallback_pid;
    OrionHEN_log("[Cheat] Module '%s' found in fallback PID %d",
                 entry->module_name, target_pid);
  }

  is_ps2 =
      (util_find_module(target_pid, "libScePs2EmuMenuDialog.sprx", &ps2_module) ==
       0);
  base_address = module_info.sections[0].vaddr;
  OrionHEN_log("[Cheat] Base address: 0x%llx (is_ps2: %d)",
               (unsigned long long)base_address, is_ps2);

  if (file->master_code_id < 0 &&
      (orion_cheat_contains_token(entry->name, "Master Code") ||
       orion_cheat_contains_token(entry->name, "Mastercode"))) {
    file->master_code_id = cheat_index;
  } else {
    fix_master_code_dependency(&target_game, file, entry, base_address,
                               &module_info, is_ps2, fw_major);
  }

  if (ops->attach != NULL && ops->attach(target_pid) < 0) {
    OrionHEN_log("[Cheat] attach failed for pid %d", target_pid);
    snprintf(status_out, status_out_size, "attach failed for pid %d",
             target_pid);
    return -1;
  }

  for (size_t i = 0; i < entry->patch_count; ++i) {
    if (result != 0) {
      break;
    }
    orion_patch_t *patch = &entry->patches[i];
    int addr_ok = 0;
    uint64_t addr = resolve_patch_address(&module_info, patch, is_ps2,
                                          base_address, &target_game, &addr_ok);
    const uint8_t *write_buf = entry->enabled ? patch->off : patch->on;
    size_t write_len = entry->enabled ? patch->off_len : patch->on_len;
    uint8_t verify[ORION_MAX_PATCH_BYTES];

    OrionHEN_log(
        "[Cheat] Patch %zu: Addr 0x%llx, Len %zu, Section %d, Absolute %d", i,
        (unsigned long long)addr, write_len, patch->section, patch->absolute);

    if (!addr_ok) {
      OrionHEN_log("[Cheat] Patch %zu: Invalid section %d", i, patch->section);
      snprintf(status_out, status_out_size, "%s -> invalid patch section %d",
               entry->name, patch->section);
      result = -1;
      break;
    }

    if (write_len == 0 || write_len > sizeof(verify)) {
      OrionHEN_log("[Cheat] Patch %zu: Invalid size %zu", i, write_len);
      snprintf(status_out, status_out_size, "%s -> invalid patch size %zu",
               entry->name, write_len);
      result = -1;
      break;
    }

    if (patch->is_asm) {
      OrionHEN_log("[Cheat] Patch %zu: ASM text not converted to machine code",
                   i);
      snprintf(status_out, status_out_size,
               "%s -> ASM text patch skipped (need assembler)", entry->name);
      result = -1;
      break;
    }

    if (!entry->enabled) {
      uint8_t before[ORION_MAX_PATCH_BYTES];
      if (ops->read(target_pid, addr, before, write_len) == 0) {
        log_patch_bytes("current before write", i, before, write_len);
        if (!patch_current_is_expected(entry, patch, before, write_len)) {
          log_patch_bytes("expected current off", i, patch->off, patch->off_len);
          OrionHEN_log(
              "[Cheat] Patch %zu: current bytes mismatch @ 0x%llx (proceeding)",
              i, (unsigned long long)addr);
        }
      }
    }

    if (ops->write(target_pid, addr, write_buf, write_len) < 0) {
      OrionHEN_log("[Cheat] Patch %zu: write failed @ 0x%llx", i,
                   (unsigned long long)addr);
      snprintf(status_out, status_out_size, "%s -> write failed @ 0x%llx",
               entry->name, (unsigned long long)addr);
      result = -1;
      break;
    }
    if (!entry->enabled) {
      bool verified = false;

      if (ops->read(target_pid, addr, verify, write_len) >= 0 &&
          patch_matches(verify, write_buf, write_len) == 0) {
        verified = true;
      }

      if (!verified && !patch->code_cave_reloc) {
        patch->code_cave_reloc = true;
        OrionHEN_log(
            "[Cheat] Patch %zu: verify failed, attempting code cave reloc @ "
            "0x%llx",
            i, (unsigned long long)addr);
        if (ops->code_cave_map(target_pid, addr, write_len) == 0 &&
            ops->write(target_pid, addr, write_buf, write_len) >= 0 &&
            ops->read(target_pid, addr, verify, write_len) >= 0 &&
            patch_matches(verify, write_buf, write_len) == 0) {
          verified = true;
          OrionHEN_log(
              "[Cheat] Patch %zu: code cave reloc retry succeeded @ 0x%llx", i,
              (unsigned long long)addr);
        }
      }

      if (!verified) {
        log_patch_bytes("expected", i, write_buf, write_len);
        log_patch_bytes("actual", i, verify, write_len);
        OrionHEN_log("[Cheat] Patch %zu: verify mismatch @ 0x%llx", i,
                     (unsigned long long)addr);
        snprintf(status_out, status_out_size, "%s -> verify mismatch @ 0x%llx",
                 entry->name, (unsigned long long)addr);
        result = -1;
        break;
      }
    }
    OrionHEN_log("[Cheat] Patch %zu: Success", i);
  }

  if (ops->detach != NULL) {
    ops->detach(target_pid, 0);
  }

  if (result == 0) {
    entry->enabled = !entry->enabled;
    enabled = entry->enabled;
    file->last_applied_pid = target_pid;
    OrionHEN_log("[Cheat] '%s' is now %s", entry->name,
                 enabled ? "ENABLED" : "DISABLED");
    snprintf(status_out, status_out_size, "%s -> %s", entry->name,
             enabled ? "enabled" : "disabled");
  } else if (status_out[0] == '\0') {
    OrionHEN_log("[Cheat] '%s' toggle failed", entry->name);
    snprintf(status_out, status_out_size, "%s -> patch verify failed",
             entry->name);
  }

  return result;
}
