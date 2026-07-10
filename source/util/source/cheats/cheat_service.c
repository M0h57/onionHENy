#include "cheats/cheat_service.h"
#include "cheats/cheat_service_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cheats/runtime.h"
#include "util_platform.h"

void OrionHEN_log(const char *fmt, ...);

static int default_file_exists(const char *path) {
  struct stat st;
  return (path != NULL && stat(path, &st) == 0) ? 1 : 0;
}

static int default_get_file_signature(const char *path, uint64_t *size_out,
                                      uint64_t *file_id_out, int64_t *mtime_out,
                                      int64_t *ctime_out) {
  struct stat st;

  if (path == NULL || size_out == NULL || file_id_out == NULL || mtime_out == NULL ||
      ctime_out == NULL) {
    return -1;
  }
  if (stat(path, &st) != 0) {
    return -1;
  }
  *size_out = (uint64_t)st.st_size;
  *file_id_out = (uint64_t)st.st_ino;
  *mtime_out = (int64_t)st.st_mtime;
  *ctime_out = (int64_t)st.st_ctime;
  return 0;
}

static void normalize_version_for_filename(char *out, size_t out_size,
                                           const char *version) {
  size_t i = 0;
  size_t j = 0;

  if (out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (version == NULL) {
    return;
  }
  for (i = 0; version[i] != '\0' && j + 1 < out_size; ++i) {
    unsigned char ch = (unsigned char)version[i];
    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= 'a' && ch <= 'z') || ch == '.' || ch == '_' || ch == '-') {
      out[j++] = (char)ch;
    } else {
      out[j++] = '_';
    }
  }
  out[j] = '\0';
}

static void try_resolve_cheat_path(char *cheat_path, size_t cheat_path_size,
                                   const char *title_id, const char *version,
                                   const char *extension) {
  const char *safe_extension = extension;

  if (cheat_path == NULL || cheat_path_size == 0 || title_id == NULL ||
      version == NULL || extension == NULL || extension[0] == '\0') {
    return;
  }
  if (safe_extension[0] == '.') {
    ++safe_extension;
  }
  snprintf(cheat_path, cheat_path_size, ORION_CHEATS_DIR "/%s_%s.%s", title_id,
           version, safe_extension);
}

static void resolve_cheat_path(char *cheat_path, size_t cheat_path_size,
                               const game_context_t *game) {
  /* Preference: json → shn → mc4 → ShnExt */
  static const char *exts[] = {"json", "shn", "mc4", "ShnExt"};
  char version[32];
  size_t i;

  if (game == NULL || game->title_id[0] == '\0' || game->version[0] == '\0' ||
      strcmp(game->version, "unknown") == 0) {
    if (cheat_path_size > 0) {
      cheat_path[0] = '\0';
    }
    return;
  }

  normalize_version_for_filename(version, sizeof(version), game->version);
  for (i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i) {
    try_resolve_cheat_path(cheat_path, cheat_path_size, game->title_id, version,
                           exts[i]);
    if (default_file_exists(cheat_path)) {
      return;
    }
  }

  /* Default candidate for error messages / missing file */
  try_resolve_cheat_path(cheat_path, cheat_path_size, game->title_id, version,
                         "json");
}

static void disable_cached_enabled_cheats(orion_cheat_service_state_t *state,
                                          const char *reason) {
  size_t i;

  if (state == NULL || !state->loaded) {
    return;
  }
  for (i = 0; i < state->cheat_file.cheat_count; ++i) {
    char status[256];
    if (!state->cheat_file.cheats[i].enabled) {
      continue;
    }
    if (orion_toggle_cheat(&state->game, &state->cheat_file, (int)i, status,
                           sizeof(status)) < 0) {
      OrionHEN_log("[service] failed to disable stale cheat index=%zu "
                       "reason=%s status=%s",
                       i, reason != NULL ? reason : "unknown", status);
    }
  }
}

static void clear_cached_cheat_file(orion_cheat_service_state_t *state) {
  if (state == NULL) {
    return;
  }
  orion_cheat_file_clear(&state->cheat_file);
  state->loaded = false;
  state->cheat_file_size = 0;
  state->cheat_file_id = 0;
  state->cheat_file_mtime = 0;
  state->cheat_file_ctime = 0;
  state->cheat_path[0] = '\0';
}

static void clear_tracked_game(orion_cheat_service_state_t *state) {
  if (state == NULL) {
    return;
  }
  state->has_tracked_game = false;
  state->tracked_pid = 0;
  disable_cached_enabled_cheats(state, "game exit");
  memset(&state->game, 0, sizeof(state->game));
  clear_cached_cheat_file(state);
}

void orion_cheat_service_state_init(orion_cheat_service_state_t *state) {
  if (state == NULL) {
    return;
  }
  memset(state, 0, sizeof(*state));
  state->cheat_file.master_code_id = -1;
  pthread_mutex_init(&state->lock, NULL);
}

void orion_cheat_service_state_destroy(orion_cheat_service_state_t *state) {
  if (state == NULL) {
    return;
  }
  orion_cheat_file_clear(&state->cheat_file);
  pthread_mutex_destroy(&state->lock);
}

orion_cheat_service_state_t *orion_cheat_service_state_create(void) {
  orion_cheat_service_state_t *state =
      (orion_cheat_service_state_t *)malloc(sizeof(*state));
  if (state == NULL) {
    return NULL;
  }
  orion_cheat_service_state_init(state);
  return state;
}

void orion_cheat_service_state_free(orion_cheat_service_state_t *state) {
  if (state == NULL) {
    return;
  }
  orion_cheat_service_state_destroy(state);
  free(state);
}

void orion_cheat_service_ensure_dir(void) {
  mkdir(ORION_DATA_ROOT, 0777);
  mkdir(ORION_CHEATS_DIR, 0777);
}

void orion_cheat_service_on_game_exec(orion_cheat_service_state_t *state,
                                      pid_t pid, const char *title_id,
                                      int appid) {
  game_context_t game;

  if (state == NULL) {
    return;
  }
  memset(&game, 0, sizeof(game));
  game.pid = pid;
  game.appid = appid;
  if (title_id != NULL) {
    snprintf(game.title_id, sizeof(game.title_id), "%s", title_id);
  }

  pthread_mutex_lock(&state->lock);
  disable_cached_enabled_cheats(state, "game exec");
  clear_cached_cheat_file(state);
  state->has_tracked_game = true;
  state->tracked_pid = pid;
  state->game = game;
  pthread_mutex_unlock(&state->lock);

  OrionHEN_log("[service] cheat tracked game exec title=%s pid=%d appid=%d",
                   title_id != NULL ? title_id : "?", (int)pid, appid);
}

void orion_cheat_service_on_game_exit(orion_cheat_service_state_t *state,
                                      pid_t pid) {
  if (state == NULL) {
    return;
  }
  pthread_mutex_lock(&state->lock);
  if (!state->has_tracked_game || state->tracked_pid != pid) {
    pthread_mutex_unlock(&state->lock);
    return;
  }
  OrionHEN_log("[service] cheat tracked game exit title=%s pid=%d",
                   state->game.title_id, (int)pid);
  clear_tracked_game(state);
  pthread_mutex_unlock(&state->lock);
}

static int fill_game_context(game_context_t *game, const char *title_id,
                             const char *version, int pid, int appid) {
  if (game == NULL || title_id == NULL || title_id[0] == '\0') {
    return -1;
  }
  memset(game, 0, sizeof(*game));
  snprintf(game->title_id, sizeof(game->title_id), "%s", title_id);
  game->pid = pid;
  game->appid = appid;
  util_game_platform_from_title_id(title_id, game->platform,
                                   sizeof(game->platform));

  if (version != NULL && version[0] != '\0' &&
      strcmp(version, "unknown") != 0) {
    snprintf(game->version, sizeof(game->version), "%s", version);
  } else if (util_resolve_game_version(title_id, game->version,
                                       sizeof(game->version)) < 0) {
    snprintf(game->version, sizeof(game->version), "unknown");
  }

  /* Prefer live BigApp process when it matches this title. */
  if (pid > 0) {
    game_context_t live;
    if (util_get_running_bigapp(&live) == 0 &&
        strcmp(live.title_id, title_id) == 0) {
      snprintf(game->process_name, sizeof(game->process_name), "%s",
               live.process_name);
      if (game->appid == 0) {
        game->appid = live.appid;
      }
      if (game->pid <= 0) {
        game->pid = live.pid;
      }
    } else {
      /* Best-effort process name from given pid */
      extern int sceKernelGetProcessName(int p, char *name);
      sceKernelGetProcessName(pid, game->process_name);
    }
  }

  return 0;
}

static int refresh_for_game(orion_cheat_service_state_t *state,
                            const game_context_t *game) {
  char cheat_path[256];
  uint64_t cheat_file_size = 0;
  uint64_t cheat_file_id = 0;
  int64_t cheat_file_mtime = 0;
  int64_t cheat_file_ctime = 0;
  int needs_reload = 0;

  if (state == NULL || game == NULL) {
    return -1;
  }

  /* Keep previously resolved path when version is unknown but title matches. */
  if (state->loaded && state->cheat_path[0] != '\0' &&
      strcmp(state->game.title_id, game->title_id) == 0 &&
      (game->version[0] == '\0' || strcmp(game->version, "unknown") == 0)) {
    state->game.pid = game->pid;
    state->game.appid = game->appid;
    return 0;
  }

  state->game = *game;
  resolve_cheat_path(cheat_path, sizeof(cheat_path), game);
  if (cheat_path[0] == '\0' || !default_file_exists(cheat_path)) {
    disable_cached_enabled_cheats(state, "cheat path unresolved");
    clear_cached_cheat_file(state);
    return -1;
  }

  if (default_get_file_signature(cheat_path, &cheat_file_size, &cheat_file_id,
                                 &cheat_file_mtime, &cheat_file_ctime) < 0) {
    disable_cached_enabled_cheats(state, "cheat stat failed");
    clear_cached_cheat_file(state);
    return -1;
  }

  needs_reload = !state->loaded || strcmp(state->cheat_path, cheat_path) != 0 ||
                 state->cheat_file_size != cheat_file_size ||
                 state->cheat_file_id != cheat_file_id ||
                 state->cheat_file_mtime != cheat_file_mtime ||
                 state->cheat_file_ctime != cheat_file_ctime;

  if (needs_reload) {
    disable_cached_enabled_cheats(state, "cheat file reload");
    orion_cheat_file_clear(&state->cheat_file);
    if (orion_load_cheat_file(cheat_path, &state->cheat_file) < 0) {
      clear_cached_cheat_file(state);
      return -1;
    }
    snprintf(state->cheat_path, sizeof(state->cheat_path), "%s", cheat_path);
    state->cheat_file_size = cheat_file_size;
    state->cheat_file_id = cheat_file_id;
    state->cheat_file_mtime = cheat_file_mtime;
    state->cheat_file_ctime = cheat_file_ctime;
    state->loaded = true;
  }

  return 0;
}

static void json_escape(const char *in, char *out, size_t out_size) {
  size_t j = 0;
  if (out_size == 0) {
    return;
  }
  out[0] = '\0';
  if (in == NULL) {
    return;
  }
  for (size_t i = 0; in[i] != '\0' && j + 2 < out_size; ++i) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      if (j + 3 >= out_size) {
        break;
      }
      out[j++] = '\\';
      out[j++] = c;
    } else if ((unsigned char)c < 0x20) {
      continue;
    } else {
      out[j++] = c;
    }
  }
  out[j] = '\0';
}

static int write_list_json(const orion_cheat_file_t *file, const char *out_path) {
  FILE *fp = NULL;
  char name_esc[256];
  char desc_esc[512];
  char file_name_esc[256];

  if (file == NULL || out_path == NULL) {
    return -1;
  }

  unlink(out_path);
  fp = fopen(out_path, "w");
  if (fp == NULL) {
    return -1;
  }

  json_escape(file->name, file_name_esc, sizeof(file_name_esc));
  fprintf(fp, "{\"name\":\"%s\",\"authors\":[],\"cheats\":[", file_name_esc);
  for (size_t i = 0; i < file->cheat_count; ++i) {
    json_escape(file->cheats[i].name, name_esc, sizeof(name_esc));
    json_escape(file->cheats[i].description, desc_esc, sizeof(desc_esc));
    fprintf(fp,
            "%s{\"name\":\"%s\",\"id\":%zu,\"enabled\":%s,\"description\":\"%s\"}",
            i == 0 ? "" : ",", name_esc, i,
            file->cheats[i].enabled ? "true" : "false", desc_esc);
  }
  fprintf(fp, "]}");
  fclose(fp);
  return 0;
}

int orion_cheat_service_export_list(orion_cheat_service_state_t *state,
                                    const char *title_id, const char *version,
                                    int pid, int appid, const char *out_path) {
  game_context_t game;
  int rc;

  if (state == NULL || title_id == NULL || out_path == NULL) {
    return -1;
  }

  if (fill_game_context(&game, title_id, version, pid, appid) < 0) {
    return -1;
  }

  pthread_mutex_lock(&state->lock);
  rc = refresh_for_game(state, &game);
  if (rc < 0) {
    pthread_mutex_unlock(&state->lock);
    return -1;
  }
  rc = write_list_json(&state->cheat_file, out_path);
  pthread_mutex_unlock(&state->lock);
  return rc;
}

int orion_cheat_service_toggle_index(orion_cheat_service_state_t *state, int pid,
                                     int appid, const char *title_id,
                                     const char *version, int index,
                                     char *status_out, size_t status_out_size) {
  game_context_t game;
  int rc;

  if (state == NULL || title_id == NULL || status_out == NULL ||
      status_out_size == 0) {
    return -1;
  }
  status_out[0] = '\0';

  if (fill_game_context(&game, title_id, version, pid, appid) < 0) {
    snprintf(status_out, status_out_size, "invalid game context");
    return -1;
  }
  if (pid > 0) {
    game.pid = pid;
  }
  if (appid != 0) {
    game.appid = appid;
  }

  pthread_mutex_lock(&state->lock);
  rc = refresh_for_game(state, &game);
  if (rc < 0) {
    pthread_mutex_unlock(&state->lock);
    snprintf(status_out, status_out_size, "unable to load cheat file");
    return -1;
  }
  if (index < 0 || (size_t)index >= state->cheat_file.cheat_count) {
    pthread_mutex_unlock(&state->lock);
    snprintf(status_out, status_out_size, "invalid cheat index %d", index);
    return -1;
  }

  /* Keep pid/appid from caller for write path. */
  state->game.pid = game.pid;
  state->game.appid = game.appid;

  rc = orion_toggle_cheat(&state->game, &state->cheat_file, index, status_out,
                          status_out_size);
  pthread_mutex_unlock(&state->lock);
  return rc;
}
