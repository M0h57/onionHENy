/* Copyright (C) 2025 OrionHEN / LightningMods */

#include <orion/playtime.h>
#include <orion/fs.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

extern void OrionHEN_log(const char *fmt, ...);

bool orion_playtime_write_record(const char *filename, const char *tid,
                                 uint64_t duration) {
  FILE *file;
  char tid_padded[ORION_PLAYTIME_TID_SIZE];

  if (!filename || !tid)
    return false;

  file = fopen(filename, "a+b");
  if (file == NULL) {
    OrionHEN_log("Failed to open file for writing: %s", strerror(errno));
    return false;
  }

  memset(tid_padded, 0, sizeof(tid_padded));
  strncpy(tid_padded, tid, ORION_PLAYTIME_TID_SIZE);

  if (fwrite(tid_padded, sizeof(char), ORION_PLAYTIME_TID_SIZE, file) <
      (size_t)ORION_PLAYTIME_TID_SIZE) {
    OrionHEN_log("Failed to write TID to file: %s", strerror(errno));
    fclose(file);
    return false;
  }

  if (fwrite(&duration, sizeof(uint64_t), 1, file) < 1) {
    OrionHEN_log("Failed to write duration to file: %s", strerror(errno));
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

bool orion_playtime_modify_duration(const char *filename, const char *target_tid,
                                    uint64_t new_duration) {
  FILE *file;
  char tid[ORION_PLAYTIME_TID_SIZE];
  uint64_t duration = 0;
  bool found = false;

  if (!filename || !target_tid)
    return false;

  file = fopen(filename, "r+b");
  if (!file) {
    OrionHEN_log("Failed to open file for reading and writing: %s",
                 strerror(errno));
    return false;
  }

  while (fread(tid, sizeof(char), ORION_PLAYTIME_TID_SIZE, file) ==
         (size_t)ORION_PLAYTIME_TID_SIZE) {
    if (fread(&duration, sizeof(uint64_t), 1, file) == 1) {
      if (strncmp(tid, target_tid, ORION_PLAYTIME_TID_SIZE) == 0) {
        found = true;
        fseek(file, -((long)sizeof(uint64_t)), SEEK_CUR);
        fwrite(&new_duration, sizeof(uint64_t), 1, file);
        break;
      }
    }
  }

  fclose(file);
  return found;
}

bool orion_playtime_get_duration(const char *filename, const char *target_tid,
                                 uint64_t *duration) {
  FILE *file;
  char tid[ORION_PLAYTIME_TID_SIZE];
  bool found = false;

  if (!filename || !target_tid || !duration)
    return false;

  if (!if_exists(filename))
    return orion_playtime_write_record(filename, target_tid, *duration);

  file = fopen(filename, "rb");
  if (file == NULL) {
    OrionHEN_log("Failed to open file for reading: %s", strerror(errno));
    return false;
  }

  while (fread(tid, sizeof(char), ORION_PLAYTIME_TID_SIZE, file) ==
         (size_t)ORION_PLAYTIME_TID_SIZE) {
    if (fread((void *)duration, sizeof(uint64_t), 1, file) == 1) {
      if (strncmp(tid, target_tid, ORION_PLAYTIME_TID_SIZE) == 0) {
        found = true;
        break;
      }
    } else {
      *duration = 0;
      break;
    }
  }

  fclose(file);
  if (found)
    return true;
  return orion_playtime_write_record(filename, target_tid, 0);
}
