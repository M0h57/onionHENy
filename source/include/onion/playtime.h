/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Binary playtime store: fixed 10-byte TID + uint64 duration records.
 * Host-testable file I/O (no PS5 SDK).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** TID field width on disk (zero-padded / truncated). */
#define ONION_PLAYTIME_TID_SIZE 10

/** Append one record. Returns true on success. */
bool onion_playtime_write_record(const char *filename, const char *tid,
                                 uint64_t duration);

/**
 * Find @p target_tid and overwrite its duration with @p new_duration.
 * Returns true if a matching record was updated.
 */
bool onion_playtime_modify_duration(const char *filename, const char *target_tid,
                                    uint64_t new_duration);

/**
 * Look up duration for @p target_tid into *@p duration.
 * If file missing: creates record with current *@p duration value.
 * If file exists but tid missing: appends record with duration 0 and sets *duration=0 path via write.
 * Returns true on success (found or created).
 */
bool onion_playtime_get_duration(const char *filename, const char *target_tid,
                                 uint64_t *duration);

#ifdef __cplusplus
}
#endif
