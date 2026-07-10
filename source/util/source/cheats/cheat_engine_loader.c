#include "cheats/cheat_engine_internal.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

void OrionHEN_log(const char *fmt, ...);


/*
 * Supported formats:
 *   .json   GoldHEN/OrionHEN JSON
 *   .shn    Trainer XML
 *   .mc4    AES-256-CBC encrypted Trainer XML
 *   .ShnExt Reaper MultiTrainer (deflate + AES + optional ASM via keystone)
 *
 * Not supported: .kcf / .wmdw
 */

int orion_cheat_load_buffer(const char *format, const unsigned char *data,
                            size_t data_len, orion_cheat_file_t *out) {
  char *buf = NULL;
  int rc;

  if (format == NULL || data == NULL || data_len == 0 || out == NULL) {
    return -1;
  }
  orion_cheat_file_clear(out);

  if (strcmp(format, "shn") == 0) {
    buf = (char *)malloc(data_len + 1);
    if (buf == NULL) {
      return -1;
    }
    memcpy(buf, data, data_len);
    buf[data_len] = '\0';
    rc = orion_cheat_parse_xml_buffer(buf, out);
    orion_cheat_secure_zero(buf, data_len + 1);
    free(buf);
    return rc;
  }

  if (strcmp(format, "mc4") == 0) {
    char *xml = orion_cheat_mc4_decrypt_buffer((const char *)data, data_len);
    if (xml == NULL) {
      return -1;
    }
    rc = orion_cheat_parse_xml_buffer(xml, out);
    orion_cheat_secure_zero(xml, strlen(xml));
    free(xml);
    return rc;
  }

  if (strcasecmp(format, "shnext") == 0 || strcasecmp(format, "ShnExt") == 0) {
    return orion_cheat_parse_shnext_buffer((const char *)data, data_len, out);
  }

  return orion_cheat_parse_json_buffer((const char *)data, data_len, out);
}

int orion_cheat_load_file(const char *path, orion_cheat_file_t *out) {
  long size = 0;
  char *buf = NULL;
  const char *ext = NULL;
  int rc;

  orion_cheat_file_clear(out);
  if (path == NULL) {
    return -1;
  }

  OrionHEN_log("[engine] load path=%s", path);
  buf = orion_cheat_load_file_buffer(path, &size);
  if (buf == NULL) {
    OrionHEN_log("[engine] load failed path=%s",
                     path);
    return -1;
  }

  ext = strrchr(path, '.');
  if (ext != NULL && strcmp(ext, ".shn") == 0) {
    rc = orion_cheat_parse_xml_buffer(buf, out);
  } else if (ext != NULL && strcmp(ext, ".mc4") == 0) {
    char *xml = orion_cheat_mc4_decrypt_buffer(buf, (size_t)size);
    orion_cheat_secure_zero(buf, (size_t)size);
    free(buf);
    if (xml == NULL) {
      OrionHEN_log("[engine] mc4 decrypt failed");
      return -1;
    }
    rc = orion_cheat_parse_xml_buffer(xml, out);
    orion_cheat_secure_zero(xml, strlen(xml));
    free(xml);
    OrionHEN_log("[engine] mc4 cheats=%zu rc=%d",
                     out->cheat_count, rc);
    return rc;
  } else if (ext != NULL && strcasecmp(ext, ".shnext") == 0) {
    rc = orion_cheat_parse_shnext_buffer(buf, (size_t)size, out);
    OrionHEN_log("[engine] shnext cheats=%zu rc=%d", out->cheat_count, rc);
  } else {
    /* .json or unknown extension → JSON */
    rc = orion_cheat_parse_json_buffer(buf, (size_t)size, out);
  }

  orion_cheat_secure_zero(buf, (size_t)size);
  free(buf);
  OrionHEN_log("[engine] loaded cheats=%zu rc=%d",
                   out->cheat_count, rc);
  return rc;
}
