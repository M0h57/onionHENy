#pragma once

#include <stddef.h>

/* Flat cheat directory only. */
#define ONION_DATA_ROOT "/data/OnionHEN"
#define ONION_CHEATS_DIR ONION_DATA_ROOT "/cheats"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * If @p name ends with a known cheat extension (.json/.shn/.mc4/.ShnExt),
 * write the format tag ("json", "shn", "mc4", "ShnExt") to @p ext_out and
 * return 1. Otherwise return 0.
 */
int onion_cheat_match_ext(const char *name, char *ext_out, size_t ext_out_size);

/**
 * Build flat install name TITLEID_VERSION.ext from GoldHEN/OnionHEN-style
 * filenames (e.g. CUSA05786_01.04_eboot.bin.json → CUSA05786_01.04.json).
 * Returns 0 on success, -1 if the name is not a recognized cheat file.
 */
int onion_cheat_build_flat_name(const char *filename, char *out, size_t out_size);

/**
 * Sanitize game version for cheat path segments: keep alnum . _ -;
 * replace other characters with '_'. Empty/NULL → empty string.
 */
void onion_cheat_normalize_version(const char *version, char *out,
                                   size_t out_size);

/** Walk @p root and copy cheats into ONION_CHEATS_DIR as flat names. */
int onion_cheat_flatten_install_tree(const char *root);

#ifdef __cplusplus
}
#endif
