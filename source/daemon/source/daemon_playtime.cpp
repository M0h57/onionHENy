/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Playtime tracking thread — extracted from commands.cpp.
 */

#include "daemon_ops.hpp"

#include <orion/platform.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

namespace {

constexpr int kMaxTidSize = 10;

bool writeRecord(const char *filename, const char *tid, uint64_t duration) {
  FILE *file = fopen(filename, "a+b");
  if (file == NULL) {
    OrionHEN_log("Failed to open file for writing: %s", strerror(errno));
    return false;
  }

  char tid_padded[kMaxTidSize] = {0};
  strncpy(tid_padded, tid, kMaxTidSize);

  if (fwrite(tid_padded, sizeof(char), kMaxTidSize, file) < (size_t)kMaxTidSize) {
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

bool modifyRecordDuration(const char *filename, const char *target_tid,
                          uint64_t &new_duration) {
  FILE *file = fopen(filename, "r+b");
  if (!file) {
    OrionHEN_log("Failed to open file for reading and writing: %s", strerror(errno));
    return false;
  }

  char tid[kMaxTidSize];
  uint64_t duration = 0;
  bool found = false;

  while (fread(tid, sizeof(char), kMaxTidSize, file) == (size_t)kMaxTidSize) {
    if (fread(&duration, sizeof(uint64_t), 1, file) == 1) {
      if (strncmp(tid, target_tid, kMaxTidSize) == 0) {
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

bool getDurationForTID(const char *filename, const char *target_tid,
                       uint64_t &duration) {
  if (!if_exists(filename))
    return writeRecord(filename, target_tid, duration);

  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    OrionHEN_log("Failed to open file for reading: %s", strerror(errno));
    return false;
  }

  char tid[kMaxTidSize];
  bool found = false;

  while (fread(tid, sizeof(char), kMaxTidSize, file) == (size_t)kMaxTidSize) {
    if (fread((void *)&duration, sizeof(uint64_t), 1, file) == 1) {
      if (strncmp(tid, target_tid, kMaxTidSize) == 0) {
        found = true;
        break;
      }
    } else {
      duration = 0;
      break;
    }
  }

  fclose(file);
  return found ? true : writeRecord(filename, target_tid, 0);
}

} // namespace

void *Play_time_thread(void *args) noexcept {
  (void)args;
  const char *filename = "/data/OrionHEN/playtime.bin";
  std::string tid;
  uint64_t duration = 0;
  int appid = 0;

  while (true) {
    if (!Get_Running_App_TID(tid, appid))
      continue;

    OrionHEN_log("getting duration for %s", tid.c_str());
    if (!getDurationForTID(filename, tid.c_str(), duration))
      continue;

    OrionHEN_log("got duration for %s: %llu", tid.c_str(),
                 (unsigned long long)duration);
    duration++;
    if (!modifyRecordDuration(filename, tid.c_str(), duration)) {
      OrionHEN_log("Failed to modify record duration for %s", tid.c_str());
      continue;
    }

    OrionHEN_log("Record duration for %s changed to %llu", tid.c_str(),
                 (unsigned long long)duration);
    sleep(59);
  }

  return nullptr;
}
