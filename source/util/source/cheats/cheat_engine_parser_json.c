#include "cheats/cheat_engine_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_memory_object(const char *start, const char *end,
                               orion_patch_t *patch) {
  char value[512];

  memset(patch, 0, sizeof(*patch));
  patch->section = 0;
  patch->absolute = false;

  if (orion_cheat_extract_scalar(start, end, "offset", value, sizeof(value)) <
      0) {
    return -1;
  }
  patch->offset = strtoull(value, NULL, 16);

  if (orion_cheat_extract_string(start, end, "on", value, sizeof(value)) < 0 ||
      orion_cheat_hex_decode(value, patch->on, sizeof(patch->on),
                             &patch->on_len) < 0) {
    return -1;
  }
  if (orion_cheat_extract_string(start, end, "off", value, sizeof(value)) < 0 ||
      orion_cheat_hex_decode(value, patch->off, sizeof(patch->off),
                             &patch->off_len) < 0) {
    return -1;
  }

  if (orion_cheat_extract_scalar(start, end, "section", value,
                                 sizeof(value)) == 0) {
    int section = atoi(value);
    if (section < MODULE_INFO_MAX_SECTIONS) {
      patch->section = section;
    }
  }
  if (orion_cheat_extract_scalar(start, end, "absolute", value,
                                 sizeof(value)) == 0) {
    patch->absolute = (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
                       strcmp(value, "\"1\"") == 0);
  }

  return 0;
}

static int parse_mod_object(const char *start, const char *end,
                            const char *process_name,
                            orion_cheat_entry_t *entry) {
  const char *memory = orion_cheat_find_key(start, end, "memory");
  const char *arr_end = NULL;
  const char *p = NULL;

  memset(entry, 0, sizeof(*entry));
  if (orion_cheat_extract_string(start, end, "name", entry->name,
                                 sizeof(entry->name)) < 0) {
    return -1;
  }
  orion_cheat_extract_string(start, end, "description", entry->description,
                             sizeof(entry->description));
  snprintf(entry->module_name, sizeof(entry->module_name), "%s", process_name);

  if (memory == NULL) {
    return 0;
  }
  memory = orion_cheat_skip_ws(memory, end);
  if (memory >= end || *memory != '[') {
    return -1;
  }

  arr_end = orion_cheat_find_matching(memory, end, '[', ']');
  if (arr_end == NULL) {
    return -1;
  }

  p = memory + 1;
  while (p < arr_end) {
    if (*p == '{') {
      const char *obj_end =
          orion_cheat_find_matching(p, arr_end + 1, '{', '}');

      if (obj_end == NULL) {
        return -1;
      }
      if (orion_cheat_entry_ensure_patch(entry) != 0) {
        return -1;
      }
      if (parse_memory_object(p, obj_end + 1,
                              &entry->patches[entry->patch_count]) == 0) {
        ++entry->patch_count;
      }
      p = obj_end + 1;
      continue;
    }
    ++p;
  }

  return 0;
}

/**
 * 解析 JSON 格式的作弊码数据。
 * 提取进程名、游戏名和 mods 数组中的作弊条目及内存补丁。
 *
 * @param json JSON 数据的起始指针。
 * @param size JSON 数据的大小。
 * @param out 指向输出结构体的指针，用于存储解析后的作弊码数据。
 * @return 成功返回 0，失败或未找到作弊码返回 -1。
 */
int orion_cheat_parse_json_buffer(const char *json, size_t size,
                                  orion_cheat_file_t *out) {
  const char *mods = NULL;
  const char *mods_end = NULL;
  const char *p = NULL;

  orion_cheat_file_clear(out);

  if (orion_cheat_extract_string(json, json + size, "process", out->process,
                                 sizeof(out->process)) < 0 ||
      orion_cheat_extract_string(json, json + size, "name", out->name,
                                 sizeof(out->name)) < 0) {
    return -1;
  }

  mods = orion_cheat_find_key(json, json + size, "mods");
  if (mods == NULL) {
    return -1;
  }

  mods = orion_cheat_skip_ws(mods, json + size);
  if (*mods != '[') {
    return -1;
  }

  mods_end = orion_cheat_find_matching(mods, json + size, '[', ']');
  if (mods_end == NULL) {
    return -1;
  }

  p = mods + 1;
  while (p < mods_end) {
    if (*p == '{') {
      const char *obj_end =
          orion_cheat_find_matching(p, mods_end + 1, '{', '}');

      if (obj_end == NULL) {
        break;
      }
      if (orion_cheat_file_ensure_cheat(out) != 0) {
        break;
      }
      orion_cheat_entry_t *entry = &out->cheats[out->cheat_count];
      if (parse_mod_object(p, obj_end + 1, out->process, entry) == 0 &&
          entry->patch_count > 0) {
        ++out->cheat_count;
      }
      p = obj_end + 1;
      continue;
    }
    ++p;
  }

  return out->cheat_count > 0 ? 0 : -1;
}
