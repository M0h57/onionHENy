#include "cheats/runtime.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void OrionHEN_log(const char *fmt, ...);

static int is_cheat_ext(const char *name, char *ext_out, size_t ext_out_size) {
  static const char *exts[] = {".json", ".shn", ".mc4", ".ShnExt", ".shnext"};
  size_t n = strlen(name);

  for (size_t i = 0; i < sizeof(exts) / sizeof(exts[0]); ++i) {
    size_t el = strlen(exts[i]);
    if (n > el && strcasecmp(name + n - el, exts[i]) == 0) {
      if (ext_out && ext_out_size) {
        if (strcasecmp(exts[i], ".shnext") == 0 ||
            strcasecmp(exts[i], ".ShnExt") == 0) {
          snprintf(ext_out, ext_out_size, "ShnExt");
        } else {
          snprintf(ext_out, ext_out_size, "%s", exts[i] + 1);
        }
      }
      return 1;
    }
  }
  return 0;
}

/*
 * Build flat name TITLEID_VERSION.ext from GoldHEN/OrionHEN style filenames:
 *   CUSA05786_01.04.json
 *   CUSA05786_01.04_eboot.bin.json
 *   PPSA01340_01.004.000.shn
 */
static int build_flat_name(const char *filename, char *out, size_t out_size) {
  char base[256];
  char ext[16];
  char title_id[32];
  char version[64];
  const char *sep = NULL;
  const char *vstart = NULL;
  const char *vend = NULL;
  size_t i = 0;

  if (!is_cheat_ext(filename, ext, sizeof(ext))) {
    return -1;
  }

  snprintf(base, sizeof(base), "%s", filename);
  /* strip extension from base */
  {
    char *dot = strrchr(base, '.');
    if (dot == NULL) {
      return -1;
    }
    /* handle .mc4.xml leftover */
    if (strcasecmp(dot, ".xml") == 0) {
      *dot = '\0';
      dot = strrchr(base, '.');
      if (dot == NULL) {
        return -1;
      }
    }
    *dot = '\0';
  }

  /* title id ends at first '_' / '-' / ' ' */
  sep = base;
  while (*sep && *sep != '_' && *sep != '-' && *sep != ' ') {
    ++sep;
  }
  if (*sep == '\0' || (size_t)(sep - base) < 4) {
    return -1;
  }
  if ((size_t)(sep - base) >= sizeof(title_id)) {
    return -1;
  }
  memcpy(title_id, base, (size_t)(sep - base));
  title_id[sep - base] = '\0';

  vstart = sep + 1;
  /* version: digits and dots until next '_' or end */
  vend = vstart;
  while (*vend &&
         (( *vend >= '0' && *vend <= '9') || *vend == '.' || *vend == 'x' ||
          *vend == 'X')) {
    ++vend;
  }
  if (vend == vstart) {
    return -1;
  }
  if ((size_t)(vend - vstart) >= sizeof(version)) {
    return -1;
  }
  memcpy(version, vstart, (size_t)(vend - vstart));
  version[vend - vstart] = '\0';

  /* uppercase title id for consistency */
  for (i = 0; title_id[i]; ++i) {
    title_id[i] = (char)toupper((unsigned char)title_id[i]);
  }

  snprintf(out, out_size, "%s_%s.%s", title_id, version, ext);
  return 0;
}

static int copy_file(const char *src, const char *dst) {
  FILE *in = fopen(src, "rb");
  FILE *out = NULL;
  char buf[8192];
  size_t n;

  if (in == NULL) {
    return -1;
  }
  out = fopen(dst, "wb");
  if (out == NULL) {
    fclose(in);
    return -1;
  }
  while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
    if (fwrite(buf, 1, n, out) != n) {
      fclose(in);
      fclose(out);
      return -1;
    }
  }
  fclose(in);
  fclose(out);
  return 0;
}

static void walk_and_flatten(const char *dir, int *copied, int *skipped) {
  DIR *d = opendir(dir);
  struct dirent *ent;

  if (d == NULL) {
    return;
  }
  while ((ent = readdir(d)) != NULL) {
    char path[512];
    char flat[256];
    char dest[512];
    struct stat st;

    if (ent->d_name[0] == '.') {
      continue;
    }
    snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
    if (stat(path, &st) != 0) {
      continue;
    }
    if (S_ISDIR(st.st_mode)) {
      walk_and_flatten(path, copied, skipped);
      continue;
    }
    if (!S_ISREG(st.st_mode)) {
      continue;
    }
    if (build_flat_name(ent->d_name, flat, sizeof(flat)) < 0) {
      continue;
    }
    snprintf(dest, sizeof(dest), ORION_CHEATS_DIR "/%s", flat);
    if (strcmp(path, dest) == 0) {
      continue;
    }
    if (copy_file(path, dest) == 0) {
      (*copied)++;
      OrionHEN_log("[flatten] %s -> %s", path, dest);
    } else {
      (*skipped)++;
    }
  }
  closedir(d);
}

/**
 * Walk a tree (typically after zip extract) and install flat cheat files into
 * ORION_CHEATS_DIR as <TITLE_ID>_<VERSION>.<ext>.
 */
int orion_cheat_flatten_install_tree(const char *root) {
  int copied = 0;
  int skipped = 0;

  mkdir(ORION_DATA_ROOT, 0777);
  mkdir(ORION_CHEATS_DIR, 0777);

  if (root == NULL || root[0] == '\0') {
    root = ORION_CHEATS_DIR;
  }
  walk_and_flatten(root, &copied, &skipped);
  OrionHEN_log("[flatten] installed %d cheat file(s), skipped %d", copied,
                   skipped);
  return copied > 0 ? 0 : -1;
}
