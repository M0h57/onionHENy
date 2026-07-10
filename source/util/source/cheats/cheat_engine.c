#include "cheats/cheat_engine.h"

#include <string.h>

#include "cheats/cheat_engine_internal.h"

/**
 * 从文件加载作弊码数据。
 * 根据文件扩展名自动选择解析方式（.shn XML / .mc4 加密 XML / JSON）。
 *
 * @param path 作弊码文件的路径。
 * @param out 指向输出结构体的指针，用于存储解析后的作弊码数据。
 * @return 成功返回 0，失败返回 -1。
 */
int orion_load_cheat_file(const char *path, orion_cheat_file_t *out) {
  return orion_cheat_load_file(path, out);
}

/**
 * 切换指定作弊码的启用状态。
 * 支持自动重置状态（游戏进程重启时）和模块回退查找。
 *
 * @param game 当前游戏上下文的指针。
 * @param file 指向作弊码文件结构体的指针。
 * @param cheat_index 待切换的作弊码索引。
 * @param status_out 输出缓冲区，用于存储状态描述字符串。
 * @param status_out_size 输出缓冲区的大小。
 * @return 成功返回 0，失败返回 -1。
 */
int orion_toggle_cheat(const game_context_t *game, orion_cheat_file_t *file,
                       int cheat_index, char *status_out,
                       size_t status_out_size) {
  return orion_cheat_toggle_entry(game, file, cheat_index, status_out,
                                   status_out_size);
}

/**
 * 重置所有作弊码的启用状态。
 * 将所有作弊码标记为禁用，并清除主码 ID 和上次应用的 PID。
 *
 * @param file 指向待重置的作弊码文件结构体的指针。
 * @return 无返回值。
 */
void orion_cheat_reset_state(orion_cheat_file_t *file) {
  if (file == NULL) {
    return;
  }
  for (size_t i = 0; i < file->cheat_count; ++i) {
    file->cheats[i].enabled = false;
  }
  file->master_code_id = -1;
  file->last_applied_pid = 0;
}


