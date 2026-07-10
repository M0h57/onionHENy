#include "cheats/cheat_engine_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void OrionHEN_log(const char *fmt, ...);


unsigned char *base64_decode(const unsigned char *src, size_t len,
                             size_t *out_len);

struct AES_ctx {
  uint8_t RoundKey[240];
  uint8_t Iv[16];
};

void AES_init_ctx_iv(struct AES_ctx *ctx, const uint8_t *key,
                     const uint8_t *iv);
void AES_CBC_decrypt_buffer(struct AES_ctx *ctx, uint8_t *buf, size_t length);

static const uint8_t MC4_AES256CBC_KEY[] = "304c6528f659c766110239a51cl5dd9c";
static const uint8_t MC4_AES256CBC_IV[] = "u@}kzW2u[u(8DWar";

static const char *find_xml_tag_value(const char *start, const char *tag,
                                      char *out, size_t out_size) {
  char open_tag[64];
  char close_tag[64];
  const char *open = NULL;
  const char *close = NULL;
  size_t len = 0;

  snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
  snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

  open = strstr(start, open_tag);
  if (open == NULL) {
    out[0] = '\0';
    return NULL;
  }
  open += strlen(open_tag);
  close = strstr(open, close_tag);
  if (close == NULL) {
    out[0] = '\0';
    return NULL;
  }

  len = (size_t)(close - open);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, open, len);
  out[len] = '\0';
  return close + strlen(close_tag);
}

static int find_xml_attr(const char *start, const char *tag, const char *attr,
                         char *out, size_t out_size) {
  char marker[64];
  char attr_marker[64];
  const char *node = NULL;
  const char *val = NULL;
  const char *end = NULL;
  size_t len = 0;

  snprintf(marker, sizeof(marker), "<%s", tag);
  snprintf(attr_marker, sizeof(attr_marker), "%s=\"", attr);
  node = strstr(start, marker);
  if (node == NULL) {
    out[0] = '\0';
    return -1;
  }
  val = strstr(node, attr_marker);
  if (val == NULL) {
    out[0] = '\0';
    return -1;
  }
  val += strlen(attr_marker);
  end = strchr(val, '"');
  if (end == NULL) {
    out[0] = '\0';
    return -1;
  }
  len = (size_t)(end - val);
  if (len >= out_size) {
    len = out_size - 1;
  }
  memcpy(out, val, len);
  out[len] = '\0';
  return 0;
}

/**
 * 解密 .mc4 格式的加密作弊码数据。
 * 使用 AES-256-CBC 解密 Base64 编码的二进制数据。
 *
 * @param encoded Base64 编码的加密数据。
 * @param encoded_size 编码数据的大小。
 * @return 解密后的 XML 字符串指针（需调用者 free），失败返回 NULL。
 */
char *orion_cheat_mc4_decrypt_buffer(const char *encoded, size_t encoded_size) {
  size_t bin_size = 0;
  unsigned char *bin =
      base64_decode((const unsigned char *)encoded, encoded_size, &bin_size);
  struct AES_ctx ctx;
  uint8_t *buf = NULL;

  if (bin == NULL) {
    return NULL;
  }
  if (bin_size == 0 || (bin_size % 16) != 0) {
    orion_cheat_secure_zero(bin, bin_size);
    free(bin);
    return NULL;
  }

  buf = (uint8_t *)calloc(bin_size + 0x100 + 1, 1);
  if (buf == NULL) {
    orion_cheat_secure_zero(bin, bin_size);
    free(bin);
    return NULL;
  }
  memcpy(buf, bin, bin_size);
  orion_cheat_secure_zero(bin, bin_size);
  free(bin);

  AES_init_ctx_iv(&ctx, MC4_AES256CBC_KEY, MC4_AES256CBC_IV);
  AES_CBC_decrypt_buffer(&ctx, buf, bin_size);
  orion_cheat_secure_zero(&ctx, sizeof(ctx));
  buf[bin_size] = '\0';
  return (char *)buf;
}

/**
 * 解析 XML 格式（.shn）的作弊码数据。
 * 预处理 XML 转义字符后提取 Trainer 属性和 Cheat 条目。
 *
 * @param xml 可修改的 XML 字符串（解析过程中会进行原地替换）。
 * @param out 指向输出结构体的指针，用于存储解析后的作弊码数据。
 * @return 成功返回 0，失败返回 -1。
 */
int orion_cheat_parse_xml_buffer(char *xml, orion_cheat_file_t *out) {
  const char *cursor = xml;
  char process[128];
  char game_name[128];

  OrionHEN_log("[engine] parse_xml_cheat_buffer begin");
  orion_cheat_file_clear(out);
  orion_cheat_replace_all(xml, 65536, "&lt;", "<");
  orion_cheat_replace_all(xml, 65536, "&gt;", ">");
  orion_cheat_replace_all(xml, 65536, "\\&quot;", "\"");
  orion_cheat_replace_all(xml, 65536, "&quot;", "\"");

  if (find_xml_attr(xml, "Trainer", "Process", process, sizeof(process)) < 0 ||
      find_xml_attr(xml, "Trainer", "Game", game_name, sizeof(game_name)) < 0) {
    OrionHEN_log("[engine] parse_xml_cheat_buffer trainer attrs missing");
    return -1;
  }
  snprintf(out->process, sizeof(out->process), "%s", process);
  snprintf(out->name, sizeof(out->name), "%s", game_name);
  OrionHEN_log("[engine] parse_xml_cheat_buffer trainer process=%s game=%s",
                   out->process, out->name);

  while ((cursor = strstr(cursor, "<Cheat ")) != NULL) {
    const char *cheat_end = strstr(cursor, "</Cheat>");
    const char *line_cursor = cursor;
    char name[128];
    char description[256];

    if (orion_cheat_file_ensure_cheat(out) != 0) {
      break;
    }
    orion_cheat_entry_t *entry = &out->cheats[out->cheat_count];

    if (cheat_end == NULL) {
      OrionHEN_log("[engine] parse_xml_cheat_buffer cheat_end missing");
      break;
    }
    memset(entry, 0, sizeof(*entry));
    if (find_xml_attr(cursor, "Cheat", "Text", name, sizeof(name)) < 0) {
      OrionHEN_log("[engine] parse_xml_cheat_buffer cheat name missing");
      cursor = cheat_end + 8;
      continue;
    }
    description[0] = '\0';
    (void)find_xml_attr(cursor, "Cheat", "Description", description,
                        sizeof(description));
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    snprintf(entry->description, sizeof(entry->description), "%s", description);
    snprintf(entry->module_name, sizeof(entry->module_name), "%s", process);

    while ((line_cursor = strstr(line_cursor, "<Cheatline>")) != NULL &&
           line_cursor < cheat_end) {
      const char *line_end = strstr(line_cursor, "</Cheatline>");
      char offset[64];
      char section[32];
      char on[512];
      char off[512];
      char absolute[32];
      orion_patch_t *patch;

      if (line_end == NULL || line_end > cheat_end) {
        OrionHEN_log("[engine] parse_xml_cheat_buffer line_end missing cheat=%s",
            entry->name);
        break;
      }

      offset[0] = '\0';
      section[0] = '\0';
      on[0] = '\0';
      off[0] = '\0';
      absolute[0] = '\0';
      find_xml_tag_value(line_cursor, "Offset", offset, sizeof(offset));
      find_xml_tag_value(line_cursor, "Section", section, sizeof(section));
      find_xml_tag_value(line_cursor, "ValueOn", on, sizeof(on));
      find_xml_tag_value(line_cursor, "ValueOff", off, sizeof(off));
      find_xml_tag_value(line_cursor, "Absolute", absolute, sizeof(absolute));

      if (offset[0] == '\0' || on[0] == '\0' || off[0] == '\0') {
        OrionHEN_log("[engine] parse_xml_cheat_buffer incomplete patch cheat=%s",
            entry->name);
        line_cursor = line_end + 12;
        continue;
      }

      if (orion_cheat_entry_ensure_patch(entry) != 0) {
        OrionHEN_log("[engine] parse_xml_cheat_buffer ensure_patch failed");
        line_cursor = line_end + 12;
        continue;
      }
      patch = &entry->patches[entry->patch_count];
      memset(patch, 0, sizeof(*patch));
      patch->offset = strtoull(offset, NULL, 16);
      if (section[0] != '\0') {
        int section_num = atoi(section);
        if (section_num < MODULE_INFO_MAX_SECTIONS) {
          patch->section = section_num;
        }
      }
      patch->absolute = absolute[0] != '\0';

      for (char *p = on; *p != '\0';) {
        if (*p == '-') {
          memmove(p, p + 1, strlen(p));
        } else {
          ++p;
        }
      }
      for (char *p = off; *p != '\0';) {
        if (*p == '-') {
          memmove(p, p + 1, strlen(p));
        } else {
          ++p;
        }
      }

      if (orion_cheat_hex_decode(on, patch->on, sizeof(patch->on),
                                 &patch->on_len) == 0 &&
          orion_cheat_hex_decode(off, patch->off, sizeof(patch->off),
                                 &patch->off_len) == 0) {
        entry->patch_count++;
      }
      line_cursor = line_end + 12;
    }

    if (entry->patch_count > 0) {
      OrionHEN_log("[engine] parse_xml_cheat_buffer cheat=%s patches=%zu",
                       entry->name, entry->patch_count);
      out->cheat_count++;
    }
    cursor = cheat_end + 8;
  }

  OrionHEN_log("[engine] parse_xml_cheat_buffer done cheats=%zu",
                   out->cheat_count);
  return out->cheat_count > 0 ? 0 : -1;
}
