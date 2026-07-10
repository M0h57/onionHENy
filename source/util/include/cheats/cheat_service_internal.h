#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cheats/cheat_engine.h"
#include "cheats/cheat_service.h"

struct orion_cheat_service_state {
  bool loaded;
  bool has_tracked_game;
  pid_t tracked_pid;
  game_context_t game;
  orion_cheat_file_t cheat_file;
  char cheat_path[256];
  uint64_t cheat_file_size;
  uint64_t cheat_file_id;
  int64_t cheat_file_mtime;
  int64_t cheat_file_ctime;
  pthread_mutex_t lock;
};

void orion_cheat_service_state_init(orion_cheat_service_state_t *state);
void orion_cheat_service_state_destroy(orion_cheat_service_state_t *state);
