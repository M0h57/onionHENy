/* Copyright (C) 2025 OrionHEN / LightningMods
 *
 * Extracted from MonoUtils.cpp for module locality.
 * Dynamic settings pages are built via ps5ui::Page (fluent XML builder).
 */

#include "HookedFuncs.hpp"
#include "RemotePlay.h"
#include "external_symbols.hpp"
#include "ipc.hpp"
#include "ps5_settings_ui.hpp"
#include "toolbox_values.hpp"
#include "../../extern/cJSON/orion_cjson.hpp"

#define PIN_CODE_SIZE 30
#define ACCOUNT_ID_BASE64_SIZE 16

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <vector>
#include <string>

#include "shellui_state.hpp"
#include "toolbox_helpers.hpp"

int usbpath();
void escapeXML(std::string& input);
bool Get_Running_App_TID(std::string& title_id, int& BigAppid);

void escapeXML(std::string& input) {
  input = ps5ui::escape(input);
}

namespace {

/** Payload .elf only (OrionHEN no longer supports .plugin packages). */
template <typename G>
void append_payload_entry(G& page, const std::string& directory, const char* filename,
                          bool list_page, int& next_id) {
  if (!toolbox::is_payload_elf_name(filename))
    return;

  const std::string path = directory + "/" + filename;
  char elf_key[64] = {};
  if (!toolbox::elf_key_from_name(filename, elf_key, sizeof(elf_key))) {
    shellui_log("Skipping invalid payload name: %s", filename);
    return;
  }

  /* Confirm file is readable (ELF magic checked at launch). */
  const int fd = open(path.c_str(), O_RDONLY, 0);
  if (fd < 0) {
    shellui_log("Failed to open payload: %s", path.c_str());
    return;
  }
  close(fd);

  shellui_log("Found payload: %s key=%s", path.c_str(), elf_key);

  const std::string shown_path = toolbox::display_path_for_ui(path);
  const std::string id_prefix = list_page ? "id_payload_" : "id_auto_payload_";
  const std::string id = id_prefix + std::to_string(next_id++);

  std::string second;
  if (list_page) {
    second = "启动/停止 " + std::string(filename) + " (路径: " + shown_path +
             ") (" + elf_key + ")";
  } else {
    second = "启用/禁用 " + std::string(filename) + " 的自动启动  (" + shown_path +
             ")";
  }

  page.toggle(id, filename, /*on=*/false, second);

  PayloadEntry entry;
  entry.shellui_path = path;
  entry.tid = elf_key;
  entry.path = shown_path;
  entry.name = filename;
  entry.version = "";
  entry.id = id;
  if (list_page)
    g_ui.payloads_list.push_back(entry);
  else
    g_ui.auto_payloads_list.push_back(entry);
}

template <typename G>
void append_homebrew_game(G& page, const std::string& game_dir, const char* dir_name,
                          int random_num) {
  const std::string elf_path = game_dir + "/eboot.elf";
  if (access(elf_path.c_str(), F_OK) != 0)
    return;

#if SHELL_DEBUG == 1
  shellui_log("Found Game: %s", game_dir.c_str());
#endif

  std::string title_id, title, ver;
  const std::string shown_path = toolbox::display_path_for_ui(game_dir);
  const std::string icon_path = game_dir + "/sce_sys/icon0.png";

  GameEntry game;
  game.tid = title_id;
  game.title = title;
  game.version = ver;
  game.path = shown_path;
  game.dir_name = dir_name;
  game.icon_path = icon_path;
  game.id = "id_orionhen_pl_loader_" + title_id + "_" + std::to_string(random_num);
  g_ui.games_list.push_back(game);

  page.button(game.id, "(" + title_id + ") " + title,
              shown_path + " | 版本: " + ver, std::nullopt, icon_path);
}

std::string read_file_to_string(const char* path) {
  struct stat st {};
  if (stat(path, &st) == -1 || st.st_size <= 0) {
    shellui_log("Unable to stat file %s", path);
    return {};
  }

  const int fd = open(path, O_RDONLY);
  if (fd < 0) {
    shellui_log("Error reading %s file!", path);
    return {};
  }

  std::string buf(static_cast<size_t>(st.st_size), '\0');
  const ssize_t n = read(fd, buf.data(), buf.size());
  close(fd);

  if (n < 0 || static_cast<size_t>(n) != buf.size()) {
    shellui_log("read failed for %s", path);
    return {};
  }
  return buf;
}

template <typename G>
void append_download_cheats_block(G& page) {
  std::string repo_value = resolve_toolbox_control_value("id_selected_cheats_repo");
  if (repo_value.empty())
    repo_value = "0";
  page.list("id_selected_cheats_repo", "金手指仓库来源",
            [](ps5ui::ListBuilder& L) {
              L.item("id_selected_cheats_repo_1", "OrionHEN PS5 金手指仓库", "0")
                  .item("id_selected_cheats_repo_2", "GoldHEN PS4 金手指仓库", "1");
            },
            /*second_title=*/std::nullopt, /*value=*/repo_value)
      .button("id_dl_cheats", "下载/更新金手指",
              "从所选 GitHub 仓库下载到 /data/OrionHEN/cheats/（TITLEID_VERSION.ext）");
}

std::string join_authors(cJSON* root) {
  std::unordered_set<std::string> seen;
  std::string joined;
  bool first = true;
  cJSON* authors = orion_cjson::item(root, "authors");
  if (!cJSON_IsArray(authors))
    return joined;

  cJSON* author = nullptr;
  cJSON_ArrayForEach(author, authors) {
    const char* value = orion_cjson::string_value(author);
    if (!value || !seen.insert(value).second)
      continue;
    if (!first)
      joined += ", ";
    joined += value;
    first = false;
  }
  return joined;
}

template <typename G>
void append_cheat_entries(G& page, cJSON* root, const std::string& tid,
                          const std::string& game_name, bool can_toggle) {
  cJSON* cheats = orion_cjson::item(root, "cheats");
  if (!cJSON_IsArray(cheats))
    return;

  cJSON* entry = nullptr;
  cJSON_ArrayForEach(entry, cheats) {
    const std::string name = orion_cjson::string_item(entry, "name", "");
    const std::string desc = orion_cjson::string_item(entry, "description", "开/关");
    const int id = orion_cjson::int_item(entry, "id");
    const bool enabled = orion_cjson::bool_item(entry, "enabled");
    const std::string id_attr = "id_cheat_" + tid + "_" + std::to_string(id);

    if (can_toggle) {
      page.toggle(id_attr, name, enabled, std::nullopt, desc, "tex_game_icon");
    } else {
      page.button(id_attr, name, "为 " + game_name + " 启用/禁用 " + name, desc,
                  "tex_game_icon");
    }
    g_ui.set_cheat_enabled(id, enabled);
  }
}

} // namespace

void generate_remote_play_xml(std::string& xml_buffer) {
  char pin_code[PIN_CODE_SIZE] = {0};
  char AccountID[ACCOUNT_ID_BASE64_SIZE] = {0};
  uint64_t dec_account_id = 0;
  bzero(AccountID, ACCOUNT_ID_BASE64_SIZE);

  shellui_log("Starting remote play");
  static bool remote_play_initialized = false;
  if (!remote_play_initialized) {
    InitRemotePlay();
    remote_play_initialized = true;
  }

  ps5ui::Page page("remote_play_pin_display", "远程游玩连接详情");
  page.root_style(ps5ui::Style::Center);

  if (IsNotActivated()) {
    GetEncodedAccountID(AccountID, dec_account_id);
    page.label("id_pin_2",
               "账号已由 OrionHEN 激活，请重启主机后再使用远程游玩！",
               ps5ui::Style::Center);
    xml_buffer = page.build();
    return;
  }

  shellui_log("Get encoded account id");
  GetEncodedAccountID(AccountID, dec_account_id);
  shellui_log("Get encoded account id ==> %s", AccountID);

  g_ui.remote_play_info = "账号 ID: " + std::string(AccountID);
  {
    std::stringstream ss;
    ss << std::hex << std::uppercase << dec_account_id;
    g_ui.remote_play_info += "\n解码后账号 ID: " + ss.str();
  }

  const uint32_t pinCode = GeneratePINCode();
  shellui_log("Pin code => %d", pinCode);
  sprintf(pin_code, "PIN 码  : %04d %04d    ", pinCode / 10000, pinCode % 10000);
  g_ui.remote_play_info += "\n" + std::string(pin_code);
  shellui_log("Pin code str => %s", pin_code);

  page.label("id_pin", pin_code, ps5ui::Style::Center)
      .label("base64_account_id", std::string("账号 ID: ") + AccountID,
             ps5ui::Style::Center);

  if (usbpath() != -1)
    page.button("id_save_rp_info", "将远程游玩详情保存到 USB", std::nullopt,
                std::nullopt, std::nullopt, ps5ui::Style::Center);

  xml_buffer = page.build();
}

void generate_payload_xml(std::string& xml_buffer, bool list_page) {
  static const std::vector<std::string> kPayloadDirs = {
      "/user/data/OrionHEN/payloads",
      "/data/OrionHEN/payloads",
      "/usb0/OrionHEN/payloads",
      "/usb1/OrionHEN/payloads",
      "/usb2/OrionHEN/payloads",
      "/usb3/OrionHEN/payloads",
  };

  const char* root_id = list_page ? "id_payload" : "id_auto_payloads";
  const char* root_title = list_page ? "Payload" : "★ Payload 自动启动";
  ps5ui::Page page(root_id, root_title);

  int toggle_switch_id = 1;
  for (const auto& directory : kPayloadDirs) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
      shellui_log("Failed to open directory: %s", directory.c_str());
      continue;
    }
    while (struct dirent* entry = readdir(dir))
      append_payload_entry(page, directory, entry->d_name, list_page,
                           toggle_switch_id);
    closedir(dir);
  }

  if (list_page) {
    page.link("id_auto_payloads", "★ Payload 自动启动", "auto_payloads.xml",
              "配置在加载 OrionHEN 时自动启动的 .elf（放在 payloads/）");
  }

  xml_buffer = page.build();
}

void generate_cheats_xml(std::string& new_xml, std::string& not_open_tid,
                         bool running_as_debug_settings, bool show_while_not_open) {
  const std::string list_id =
      running_as_debug_settings ? "id_debug_settings" : "id_cheat_title";

  int appid = -1;
  g_ui.is_game_open = Get_Running_App_TID(g_ui.running_tid, appid);
  g_ui.is_current_game_open =
      g_ui.is_game_open &&
      g_ui.running_tid == (show_while_not_open ? not_open_tid : g_ui.running_tid);

  if (!g_ui.is_game_open && !show_while_not_open) {
    ps5ui::Page page(list_id, "OrionHEN 金手指 - 当前没有打开的游戏");
    append_download_cheats_block(page);
    new_xml = page.build();
    return;
  }

  g_ui.running_tid = show_while_not_open ? not_open_tid : g_ui.running_tid;
  IPC_Client& client = IPC_Client::getInstance(true);

  std::string game_ver;
  if (!client.GameVerFromTid(g_ui.running_tid, game_ver))
    game_ver = "无法检测补丁版本";

  ps5ui::Page page(list_id,
                   "OrionHEN 金手指 - " + g_ui.running_tid + " - " + game_ver);

  if (!g_ui.is_game_open && show_while_not_open) {
    page.label("id_cheat_disclaimer",
               g_ui.running_tid + " 当前未运行，除非打开游戏否则无法激活任何金手指",
               ps5ui::Style::Center);
  }

  std::string cheat_path;
  if (!client.GetGameCheats(g_ui.running_tid, game_ver, cheat_path)) {
    append_download_cheats_block(page);
    new_xml = page.build();
    return;
  }

  const std::string json_string = read_file_to_string(cheat_path.c_str());
  if (json_string.empty()) {
    new_xml = page.build();
    return;
  }
  unlink(cheat_path.c_str());

  orion_cjson::Root res_json(json_string);
  if (!res_json) {
    shellui_log("Failed to parse json from cheat response!");
    new_xml = page.build();
    return;
  }

  const std::string game_name =
      orion_cjson::string_item(res_json.get(), "name", "");
  page.label("id_cheat_title", "★ " + game_name + " ★", ps5ui::Style::Center);

  const std::string authors = join_authors(res_json.get());
  page.label("credits", "金手指作者: " + authors, ps5ui::Style::Center);

  append_cheat_entries(page, res_json.get(), g_ui.running_tid, game_name,
                       g_ui.is_game_open && g_ui.is_current_game_open);
  new_xml = page.build();
}

void generate_plapps_xml(std::string& new_xml) {
  static const std::vector<std::string> kHomebrewDirs = {
      "/user/data/homebrew/games",
      "/usb0/homebrew",
      "/usb1/homebrew/games",
      "/usb2/homebrew/games",
      "/usb3/homebrew/games",
      "/mnt/ext1/homebrew/games",
      "/mnt/ext2/homebrew/games",
      "/mnt/ext0/homebrew/games",
  };

  ps5ui::Page page("id_plapps", "OrionHEN Payload 自制软件 - 应用程序");

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<int> dist(1000, 9999);

  for (const auto& directory : kHomebrewDirs) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) {
#if SHELL_DEBUG == 1
      shellui_log("Failed to open directory: %s", directory.c_str());
#endif
      continue;
    }

    while (struct dirent* entry = readdir(dir)) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;

      const std::string game_dir = directory + "/" + entry->d_name;
      struct stat st {};
      if (stat(game_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
#if SHELL_DEBUG == 1
        shellui_log("Skipping non-directory: %s", game_dir.c_str());
#endif
        continue;
      }

      append_homebrew_game(page, game_dir, entry->d_name, dist(gen));
    }
    closedir(dir);
  }

  new_xml = page.build();
}

namespace {

constexpr const char* kIconPkg =
    "/user/data/OrionHEN/assets/icon_xml_package.png";
constexpr const char* kIconPlugins =
    "/user/data/OrionHEN/assets/icon_xml_plugins.png";
constexpr const char* kIconGame = "/user/data/OrionHEN/assets/icon_xml_game.png";
constexpr const char* kIconSettings =
    "/user/data/OrionHEN/assets/icon_xml_settings.png";
constexpr const char* kIconShortcuts =
    "/user/data/OrionHEN/assets/icon_xml_shortcuts.png";
constexpr const char* kIconDebug =
    "/user/data/OrionHEN/assets/icon_xml_debug.png";
constexpr const char* kIconAbout =
    "/user/data/OrionHEN/assets/icon_xml_about.png";

bool toolbox_on(const char* id) {
  return resolve_toolbox_control_value(id) == "1";
}

std::string toolbox_val(const char* id, const char* fallback = "0") {
  std::string v = resolve_toolbox_control_value(id);
  return v.empty() ? fallback : v;
}

void append_toolbox_pkg_group(ps5ui::Group& g) {
  g.link("id_game_package_installer", "软件包安装器",
         "PkgInstaller/data/pkginstaller.xml")
      .link("id_game_add_content_manager", "附加内容管理器",
            "Addcontent/data/addcontent.xml");
}

void append_toolbox_payloads_group(ps5ui::Group& g) {
  g.link("id_payloads", "Payload", "payloads.xml")
      .group(
          "id_kstuff_opts", "Kstuff",
          [](ps5ui::Group& k) {
            k.toggle("id_kstuff_autoload", "OrionHEN 启动时自动加载 Kstuff",
                     toolbox_on("id_kstuff_autoload"))
                .button("id_download_kstuff", "通过 GitHub 替换为最新 Kstuff",
                        std::nullopt,
                        "从 EchoStretch 的 GitHub 仓库下载并安装最新 kstuff，"
                        "替换 OrionHEN 启动时自动加载的版本")
                .button("id_delete_kstuff", "删除通过 GitHub 安装的 Kstuff",
                        std::nullopt, "将切换回 OrionHEN 内置的 kstuff");
          },
          "内核补丁组件管理", std::nullopt, "id_kstuff_autoload");
}

void append_toolbox_game_group(ps5ui::Group& g) {
  g.link("id_cheats", "金手指（开发中）", "cheats.xml")
      .link("remote_play", "远程游玩", "remote_play.xml")
      .toggle("id_custom_game_opts", "OrionHEN 游戏选项",
              toolbox_on("id_custom_game_opts"),
              "在游戏选项菜单中显示 OrionHEN 相关选项（金手指、转储等）")
      .group(
          "id_overlay_opts", "游戏覆盖层",
          [](ps5ui::Group& o) {
            o.list("id_overlay_change_pos", "监控条位置",
                   [](ps5ui::ListBuilder& L) {
                     L.item("id_overlay_pos_1", "顶部贴边", "0")
                         .item("id_overlay_pos_3", "底部贴边", "2");
                   },
                   "贴屏幕边缘、宽度 100%；指标居中：FPS · CPU · GPU · RAM · IP",
                   toolbox_val("id_overlay_change_pos"))
                .toggle("id_overlay_fps", "*实验性* FPS",
                        toolbox_on("id_overlay_fps"), std::nullopt,
                        "显示 FPS 段（依赖游戏内 FPS 字符串捕获，可能不稳定）")
                .toggle("id_overlay_gpu", "GPU", toolbox_on("id_overlay_gpu"),
                        std::nullopt, "显示 GPU 温度与显存占用")
                .toggle("id_overlay_cpu", "CPU", toolbox_on("id_overlay_cpu"),
                        std::nullopt, "显示 CPU 温度与平均使用率")
                .toggle("id_all_cpu_usage", "显示全部 CPU 核心使用率",
                        toolbox_on("id_all_cpu_usage"), std::nullopt,
                        "CPU 段改为 8 核分别显示（监控条会加宽）")
                .toggle("id_overlay_ram", "内存", toolbox_on("id_overlay_ram"),
                        std::nullopt, "显示系统内存占用")
                .toggle("id_overlay_ip", "IP 地址", toolbox_on("id_overlay_ip"),
                        std::nullopt, "显示主机局域网 IP");
          },
          "贴边全宽横条 + 半透明黑底（PHU flex banner）", std::nullopt,
          "id_overlay_change_pos");
}

void append_toolbox_system_group(ps5ui::Group& g) {
  g.toggle("id_disp_titleids", "在主菜单显示 Title ID",
           toolbox_on("id_disp_titleids"),
           "零售机可用，但仅在工具箱激活时显示")
      .toggle("id_auto_eject", "OrionHEN 启动时自动弹出光盘",
              toolbox_on("id_auto_eject"), std::nullopt,
              "OrionHEN 完全启动后自动弹出已插入的光盘，适用于 BD-J 或 LUA "
              "漏洞利用",
              std::nullopt, "更改将在下次重启后生效")
      .group(
          "id_group_fan", "风扇控制",
          [](ps5ui::Group& f) {
            f.toggle("id_enable_fan_speed", "启用手动风扇阈值",
                     toolbox_on("id_enable_fan_speed"))
                .text_field("id_fan_speed", "调整风扇阈值",
                            "按摄氏度调整风扇阈值", "number", "2", "2",
                            std::nullopt, std::nullopt, std::nullopt,
                            toolbox_val("id_fan_speed", ""));
          },
          std::nullopt, std::nullopt, "id_enable_fan_speed")
      .group(
          "id_rest_mode", "休息模式",
          [](ps5ui::Group& r) {
            r.text_field(
                 "id_rest_1", "延迟工具箱激活（秒）",
                 "延迟工具箱内的补丁以防止卡死（在已有内置延迟之外额外增加）",
                 "number", "1", "255", std::nullopt, std::nullopt, std::nullopt,
                 toolbox_val("id_rest_1", ""))
                .toggle(
                    "id_rest_2",
                    "进入休息模式时自动关闭 OrionHEN 服务守护进程",
                    toolbox_on("id_rest_2"),
                    "从休息模式恢复后将重新启动守护进程")
                .toggle("id_rest_3", "进入休息模式时自动关闭已打开的游戏",
                        toolbox_on("id_rest_3"),
                        "进入休息模式时尝试关闭任何已打开的游戏");
          },
          "提升休息模式稳定性", std::nullopt, "id_rest_1")
      .link("id_external_hdd", "外接硬盘",
            "DebugSettings/data/debug_settings_external_hdd.xml")
      .link("id_licenseactivation", "蓝光（许可证）激活",
            "DebugSettings/data/debug_settings_licenseactivation.xml");
}

void append_toolbox_shortcuts_group(ps5ui::Group& g) {
  g.list("id_cheats_shortcut", "打开金手指菜单",
         [](ps5ui::ListBuilder& L) {
           L.item("id_cheats_shortcut_0", "关闭（无快捷键）", "0")
               .item("id_cheats_shortcut_1", "按住 R3 + L3", "1")
               .item("id_cheats_shortcut_2", "按住 L2 + △", "2")
               .item("id_cheats_shortcut_3", "长按选项键", "3")
               .item("id_cheats_shortcut_4", "长按分享键", "4")
               .item("id_cheats_shortcut_5", "单击分享键", "5");
         },
         "从任意位置（含游戏内）打开金手指菜单",
         toolbox_val("id_cheats_shortcut"))
      .list("id_toolbox_shortcut", "打开 OrionHEN 工具箱",
            [](ps5ui::ListBuilder& L) {
              L.item("id_toolbox_shortcut_0", "关闭（无快捷键）", "0")
                  .item("id_toolbox_shortcut_1", "按住 L2 + R3", "1")
                  .item("id_toolbox_shortcut_2", "长按分享键", "2")
                  .item("id_toolbox_shortcut_3", "单击分享键", "3");
            },
            "从任意位置（含游戏内）打开工具箱",
            toolbox_val("id_toolbox_shortcut"));
}

void append_toolbox_debug_group(ps5ui::Group& g) {
  g.toggle("id_debug_jb", "应用越狱通知", toolbox_on("id_debug_jb"),
           "在越狱应用时显示通知")
      .toggle("id_debug_legacy_cmd", "旧版越狱命令服务器",
              toolbox_on("id_debug_legacy_cmd"),
              "需要网络；应用可通过 Socket 请求越狱")
      .text_field("id_np_env", "NP 环境", std::nullopt, "basic_latin", "1", "16",
                  "/NP/env", "系统将重启以应用此设置。", "确定,取消");
}

void append_toolbox_about_group(ps5ui::Group& g) {
  g.group(
       "id_donation_methods", "支持本项目",
       [](ps5ui::Group& d) {
         d.label("id_method_info", "★ 捐赠方式", ps5ui::Style::Center)
             .label("id_method_1",
                    "- GitHub Sponsors  | https://github.com/sponsors/LightningMods",
                    ps5ui::Style::Center);
       },
       "喜欢这个项目吗？欢迎捐赠支持")
      .group(
          "id_orionhen_credits", "OrionHEN 致谢",
          [](ps5ui::Group& c) {
            c.label("id_orionhen_creds_display", "★ OrionHEN Beta 2.5",
                    ps5ui::Style::Center)
                .label("id_lead_devs", "★ 主要开发者 ★", ps5ui::Style::Center)
                .label("id_lead_devs_2",
                       "- LM (X @LightningMods_, GitHub @LightningMods, Discord "
                       "@lm_dev)",
                       ps5ui::Style::Center)
                .label("id_ddkdkd", "★ OrionHEN 贡献者 ★", ps5ui::Style::Center)
                .label("id_major_line",
                       "Specter - Byepervisor          astrelsky          "
                       "ChendoChap       sleirsgoevy - Kstuff",
                       ps5ui::Style::Center)
                .label("id_major_line_3", "John tornblom - elfldr 等",
                       ps5ui::Style::Center)
                .label("id_99877777", "更多信息、更新或问题请访问 GitHub 仓库",
                       ps5ui::Style::Center)
                .label("id_99555557", "OrionHEN 项目仓库", ps5ui::Style::Center)
                .label("id_8585858", "特别感谢所有支持者，包括以下人员：",
                       ps5ui::Style::Center)
                .label("id_60606066",
                       "MODDED WARFARE, Bucanero, Echo Stretch, "
                       "CurrentGenGamesWithNick, Reo Auin, illusion, "
                       "nanospeedgamer, Nomadic, Michael Crump, Br0ken4life, "
                       "Mouuu, Newhouse-Estates, Dr Angry ",
                       ps5ui::Style::Left)
                .label("id_60888880",
                       "Richard Stoltz, Not So Typical Gamer, Doobie, MC, "
                       "WWIII, dutchfavx, Pulsar, gorshco, illix, Ya Boi "
                       "Michael, Nineof09, Lostferwords, Kevin, kUiTs, ram",
                       ps5ui::Style::Left)
                .label("id_606064330",
                       "onstar, Mapleditch, pyksy, Puky70, TheBoySassy21, "
                       "Arnooooo, Jacksun, William, MauricioRodriguez, "
                       "Micaiah, Madmac, Grit, dIGIMAN/TRSI, xe, Priyesh Patel",
                       ps5ui::Style::Left)
                .label("id_60606770",
                       "JUNGLIST, Mega, smoothcriminal, Aka3z, Btet, 星空尽头, "
                       "Tunc, Pitouuuu, Moha, proton, teotl, Hector, Osensama, "
                       "Trope, x, jack favvv, lbc, Jay, mstrdtchs",
                       ps5ui::Style::Left)
                .label("id_606069990",
                       "rookie_mx, SvenGDK, jose Gonzalez, Lysy767, Alfr3d, "
                       "Fey, Knight1701, Efrain, Hernie, Johns, Madz, CRUCHI, "
                       "koldoborne, slang777, Puffinz, Tv, Ubaldo Navarro",
                       ps5ui::Style::Left)
                .label("id_606099960",
                       "Escaflowne, SrBonet, Gauban, joao, El01unO, SrBonet, "
                       "Rayyden, CZ, Efrain De Alba, aide199a, Acesmokemall, "
                       "Mheepae3029, fresno, wiiiiiz, aln, Eli, marusa Bucicas",
                       ps5ui::Style::Left)
                .label("id_606055560",
                       "Ion Florin Berusca, Bbuster, Dimitar, ROBERHUGO, "
                       "PlayStationHaX, TecnoConsolas, "
                       "mega_lelikUAPS4_5.55, An21V1rus, cyberrep, "
                       "PlanetaryNoob, chiagre, Samwise, Fortderrick",
                       ps5ui::Style::Left)
                .label("id_6060ttttt0",
                       "ChimeFix, PeenButt, Wr0zen, Shawncarnage, Kuny, "
                       "Cruznik, Vicen, shagy #8543, pepitopajas, Jakob "
                       "Trammell, Austin Meer, scrdcow, XDOSEX, Kleei, Pif, "
                       "ajslayer",
                       ps5ui::Style::Left);
          },
          "致谢与支持者")
      .group(
          "id_inc_project", "OrionHEN 所包含的项目",
          [](ps5ui::Group& p) {
            p.label("id_project_info", "★ OrionHEN 中使用的开源与闭源项目",
                    ps5ui::Style::Center)
                .label("id_project_1",
                       "PS5 Payload Dev SDK - "
                       "https://github.com/ps5-payload-dev/sdk",
                       ps5ui::Style::Center)
                .label("id_project_2",
                       "PS5 Payload Dev elfldr - "
                       "https://github.com/ps5-payload-dev/elfldr",
                       ps5ui::Style::Center)
                .label("id_project_3",
                       "PS5 Dev Byepervisor - "
                       "https://github.com/PS5Dev/Byepervisor",
                       ps5ui::Style::Center)
                .label("id_project_4", "Kstuff by sleirsgoevy",
                       ps5ui::Style::Center)
                .label("id_project_5",
                       "7-Zip LZMA - https://www.7-zip.org/sdk.html",
                       ps5ui::Style::Center)
                .label("id_project_6",
                       "Libhijacker by astrelsky - "
                       "https://github.com/astrelsky/libhijacker",
                       ps5ui::Style::Center)
                .label("id_project_7",
                       "PS5-SELF-Decrypter by Specter - "
                       "https://github.com/Cryptogenic/PS5-SELF-Decrypter",
                       ps5ui::Style::Center)
                .label("id_project_8",
                       "PS5Debug by CTN - https://github.com/GoldHEN/ps5debug",
                       ps5ui::Style::Center)
                .label("id_project_9",
                       "LibNineS - https://github.com/buzzer-re/NineS",
                       ps5ui::Style::Center)
                .label("id_project_10",
                       "cJSON - https://github.com/DaveGamble/cJSON",
                       ps5ui::Style::Center);
          },
          "OrionHEN 中使用的项目");
}

} // namespace

void generate_toolbox_xml(std::string& new_xml) {
  ps5ui::Page page("id_debug_settings", "★OrionHEN 工具箱");
  page.root_focus("id_group_pkg");

  page.group(
          "id_group_pkg", "软件包安装",
          [](ps5ui::Group& g) { append_toolbox_pkg_group(g); },
          "安装 PKG 与附加内容", kIconPkg, "id_game_package_installer")
      .group(
          "id_group_payloads", "Payload 与内核",
          [](ps5ui::Group& g) { append_toolbox_payloads_group(g); },
          "Payload ELF 与 Kstuff", kIconPlugins, "id_payloads")
      .group(
          "id_group_game", "游戏功能",
          [](ps5ui::Group& g) { append_toolbox_game_group(g); },
          "金手指、远程游玩与游戏内覆盖层", kIconGame, "id_cheats")
      .group(
          "id_group_system", "系统设置",
          [](ps5ui::Group& g) { append_toolbox_system_group(g); },
          "权限、硬件、存储与休息模式", kIconSettings, "id_disp_titleids")
      .group(
          "id_utils", "手柄快捷键",
          [](ps5ui::Group& g) { append_toolbox_shortcuts_group(g); },
          "快捷键在重启后仍然有效，组合键不限于游戏内使用", kIconShortcuts,
          "id_cheats_shortcut")
      .group(
          "id_group_debug", "调试选项",
          [](ps5ui::Group& g) { append_toolbox_debug_group(g); },
          "越狱调试与环境", kIconDebug, "id_debug_jb")
      .group(
          "id_orionhen_credit_options", "关于",
          [](ps5ui::Group& g) { append_toolbox_about_group(g); },
          "致谢、捐赠与项目信息", kIconAbout, std::nullopt,
          ps5ui::Style::Center);

  new_xml = page.build();
}
