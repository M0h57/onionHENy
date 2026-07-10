#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cheats/cheat_engine.h"
#include "util_platform.h"

/* ---- remote memory ops (strategy pattern) ------------------------------- */

typedef struct remote_mem_ops {
  int (*read)(int pid, uint64_t addr, void *buf, size_t len);
  int (*write)(int pid, uint64_t addr, const void *buf, size_t len);
  int (*code_cave_map)(int pid, uint64_t addr, size_t len);
  int (*attach)(int pid);
  int (*detach)(int pid, int sig);
} remote_mem_ops_t;

#define REMOTE_PAGE_SIZE 0x4000ULL

static inline uint64_t cheat_page_align_down(uint64_t value) {
  return value & ~(REMOTE_PAGE_SIZE - 1ULL);
}

static inline uint64_t cheat_page_align_up(uint64_t value) {
  return (value + REMOTE_PAGE_SIZE - 1ULL) & ~(REMOTE_PAGE_SIZE - 1ULL);
}

const remote_mem_ops_t *remote_ops_for_fw(int fw_major);
const remote_mem_ops_t *cheat_mem_mdbg_ops(void);
const remote_mem_ops_t *cheat_mem_kdirect_ops(void);

/* ---- parser / util declarations ---------------------------------------- */

/**
 * 检查字符串中是否存在由空白字符分隔的标记。
 *
 * @param haystack 待搜索的字符串。
 * @param needle 要查找的标记。
 * @return 找到返回 true，否则返回 false。
 */
bool orion_cheat_contains_token(const char *haystack, const char *needle);

/**
 * 将指针前进到超过所有空白字符的位置，直到结束边界。
 *
 * @param p 缓冲区内的起始指针。
 * @param end 缓冲区的独占结束边界。
 * @return 指向第一个非空白字符的指针，若未找到则返回 end。
 */
const char *orion_cheat_skip_ws(const char *p, const char *end);
/**
 * 在作弊定义缓冲区的分隔区域内查找命名的键。
 *
 * @param start 待搜索区域的起始位置。
 * @param end 区域的独占结束位置。
 * @param key 要定位的键名。
 * @return 指向键分隔符后字符的指针，未找到返回 NULL。
 */
const char *orion_cheat_find_key(const char *start, const char *end,
                                 const char *key);
/**
 * 查找与起始括号字符匹配的结束分隔符。
 *
 * @param start 起始分隔符之后的位置。
 * @param end 缓冲区的独占结束边界。
 * @param open_ch 起始分隔符字符。
 * @param close_ch 结束分隔符字符。
 * @return 指向匹配的结束分隔符的指针，若不匹配则返回 NULL。
 */
const char *orion_cheat_find_matching(const char *start, const char *end,
                                      char open_ch, char close_ch);
/**
 * 从作弊定义区域中提取与键关联的字符串值。
 *
 * @param start 待搜索区域的起始位置。
 * @param end 区域的独占结束位置。
 * @param key 要提取字符串值的键。
 * @param out 提取的字符串的输出缓冲区。
 * @param out_size 输出缓冲区的大小。
 * @return 成功返回 0，失败返回负值。
 */
int orion_cheat_extract_string(const char *start, const char *end,
                               const char *key, char *out, size_t out_size);
/**
 * 从作弊定义区域中提取与键关联的标量（数值）值，结果为十六进制字符串。
 *
 * @param start 待搜索区域的起始位置。
 * @param end 区域的独占结束位置。
 * @param key 要提取标量值的键。
 * @param out 十六进制编码值的输出缓冲区。
 * @param out_size 输出缓冲区的大小。
 * @return 成功返回 0，失败返回负值。
 */
int orion_cheat_extract_scalar(const char *start, const char *end,
                               const char *key, char *out, size_t out_size);
/**
 * 将十六进制字符串解码为原始字节。
 *
 * @param hex 十六进制编码的输入字符串。
 * @param out 解码后字节的输出缓冲区。
 * @param max_len 最大写入字节数。
 * @param out_len 接收实际解码的字节数。
 * @return 成功返回 0，失败返回负值。
 */
int orion_cheat_hex_decode(const char *hex, uint8_t *out, size_t max_len,
                           size_t *out_len);
/**
 * 将文件的全部内容加载到 malloc 分配的缓冲区中。
 *
 * @param path 要读取的文件路径。
 * @param size_out 接收文件大小（字节）。
 * @return 成功返回缓冲区指针，失败返回 NULL。
 */
char *orion_cheat_load_file_buffer(const char *path, long *size_out);
/**
 * 替换缓冲区中所有出现的子字符串。
 *
 * @param text 要原地修改的文本缓冲区。
 * @param cap 缓冲区容量（包括 NUL 终止符）。
 * @param from 要替换的子字符串。
 * @param to 替换字符串。
 */
void orion_cheat_replace_all(char *text, size_t cap, const char *from,
                             const char *to);
void orion_cheat_secure_zero(void *ptr, size_t len);
/**
 * 解密使用 MC4 算法混淆的缓冲区。
 *
 * @param encoded 编码后的输入数据。
 * @param encoded_size 编码数据的大小。
 * @return malloc 分配的解密字符串，失败返回 NULL。
 */
char *orion_cheat_mc4_decrypt_buffer(const char *encoded, size_t encoded_size);
/**
 * 规范化 PS4 eboot 偏移量，考虑模块基地址和 PS2 模式。
 *
 * @param game 当前游戏上下文。
 * @param module 包含基地址的模块信息。
 * @param offset 要规范化的原始偏移量。
 * @param is_ps2 游戏是否使用 PS2 向后兼容模式。
 * @return 规范化的绝对虚拟地址。
 */
uint64_t orion_cheat_normalize_ps4_eboot_offset(
    const game_context_t *game, const util_module_info_t *module,
    uint64_t offset, bool is_ps2);

/**
 * 将 JSON 格式的作弊文件缓冲区解析为 orion_cheat_file_t。
 *
 * @param json 要解析的 JSON 缓冲区。
 * @param size JSON 缓冲区的大小。
 * @param out 指向输出作弊文件结构的指针。
 * @return 成功返回 0，失败返回负值。
 */
int orion_cheat_parse_json_buffer(const char *json, size_t size,
                                  orion_cheat_file_t *out);
/**
 * 将 XML 格式的作弊文件缓冲区解析为 orion_cheat_file_t。
 *
 * @param xml 要解析的 XML 缓冲区（可能会被原地修改）。
 * @param out 指向输出作弊文件结构的指针。
 * @return 成功返回 0，失败返回负值。
 */
int orion_cheat_parse_xml_buffer(char *xml, orion_cheat_file_t *out);
/**
 * 解析 ShnExt（Reaper MultiTrainer）缓冲区。
 */
int orion_cheat_parse_shnext_buffer(const char *data, size_t size,
                                    orion_cheat_file_t *out);
/**
 * 从磁盘加载并解析作弊文件（.json / .shn / .mc4 / .ShnExt）。
 */
int orion_cheat_load_file(const char *path, orion_cheat_file_t *out);
int orion_cheat_load_buffer(const char *format, const unsigned char *data,
                            size_t data_len, orion_cheat_file_t *out);
/**
 * 打开或关闭指定的作弊条目并应用更改。
 *
 * @param game 当前游戏上下文。
 * @param file 包含该条目的作弊文件。
 * @param cheat_index 要切换的作弊条目的索引。
 * @param status_out 用于输出人类可读状态字符串的缓冲区。
 * @param status_out_size 状态输出缓冲区的大小。
 * @return 成功返回 0，失败返回负值。
 */
int orion_cheat_toggle_entry(const game_context_t *game,
                             orion_cheat_file_t *file, int cheat_index,
                             char *status_out, size_t status_out_size);
