#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "cheats/cheat_engine.h"

typedef struct orion_cheat_service_state orion_cheat_service_state_t;

orion_cheat_service_state_t *orion_cheat_service_state_create(void);
void orion_cheat_service_state_free(orion_cheat_service_state_t *state);

void orion_cheat_service_on_game_exec(orion_cheat_service_state_t *state,
                                      pid_t pid, const char *title_id,
                                      int appid);
void orion_cheat_service_on_game_exit(orion_cheat_service_state_t *state,
                                      pid_t pid);

/**
 * Resolve/load cheats for title_id+version into service state.
 * Writes ShellUI-compatible JSON list to out_path.
 * Returns 0 on success.
 */
int orion_cheat_service_export_list(orion_cheat_service_state_t *state,
                                    const char *title_id, const char *version,
                                    int pid, int appid,
                                    const char *out_path);

/**
 * Toggle cheat by index for the currently loaded file.
 * status_out receives human-readable result.
 */
int orion_cheat_service_toggle_index(orion_cheat_service_state_t *state,
                                     int pid, int appid,
                                     const char *title_id,
                                     const char *version, int index,
                                     char *status_out, size_t status_out_size);

/** Ensure cheats directory exists (flat layout). */
void orion_cheat_service_ensure_dir(void);

/**
 * Flatten nested cheat repo trees into ORION_CHEATS_DIR flat names.
 * @return 0 if at least one file installed, negative on failure.
 */
int orion_cheat_flatten_install_tree(const char *root);
