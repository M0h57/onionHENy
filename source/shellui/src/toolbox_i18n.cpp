/* Copyright (C) 2025 OnionHEN / LightningMods */

#include "toolbox_i18n.hpp"

#ifndef ONION_HOST_TEST
#include "external_symbols.hpp"
#endif

#include <cstring>

namespace toolbox_i18n {
namespace {

Lang g_lang = Lang::ZhHans;

struct Entry {
  const char *key;
  const char *zh;
  const char *en;
};

// Stable keys; zh is source of truth for existing Chinese UI.
// clang-format off
constexpr Entry kTable[] = {
  // Root / groups
  {"root.title", "★OnionHEN 工具箱", "★OnionHEN Toolbox"},
  {"group.pkg", "内容安装与管理", "Content Install & Management"},
  {"group.pkg.sub", "安装 PKG 与管理附加内容", "Install PKGs and manage add-on content"},
  {"group.payloads", "Payload 与内核", "Payloads & Kernel"},
  {"group.payloads.sub", "Payload ELF 与 Kstuff", "Payload ELFs and Kstuff"},
  {"group.game", "游戏辅助", "Game Tools"},
  {"group.game.sub", "金手指与游戏菜单选项", "Cheats and game-menu options"},
  {"group.display", "监控与显示", "Monitoring & Display"},
  {"group.display.sub", "游戏覆盖层与主菜单显示", "In-game overlay and home menu display"},
  {"group.connection", "账号、连接与远程游玩", "Account, Connection & Remote Play"},
  {"group.connection.sub", "远程游玩配对与连接详情", "Remote Play pairing and connection details"},
  {"group.system", "系统与硬件", "System & Hardware"},
  {"group.system.sub", "风扇、休息模式、存储与许可证", "Fan, Rest Mode, storage, and license activation"},
  {"group.shortcuts", "手柄快捷键", "Controller Shortcuts"},
  {"group.shortcuts.sub", "快捷键在重启后仍然有效，组合键不限于游戏内使用",
   "Shortcuts persist across reboots while the toolbox is active; combos work outside games"},
  {"group.preferences", "操作偏好", "Preferences"},
  {"group.preferences.sub", "工具箱语言与手柄快捷键",
   "Toolbox language and controller shortcuts"},
  {"group.debug", "高级调试", "Advanced Debug"},
  {"group.debug.sub", "应用越狱通知与 NP 环境",
   "App jailbreak notifications and NP environment"},
  {"group.about", "关于", "About"},
  {"group.about.sub", "致谢、捐赠与项目信息", "Credits, donations, and project info"},
  {"group.lang", "界面语言", "UI Language"},
  {"group.lang.sub", "切换后请退出并重新打开工具箱",
   "Leave and re-open the toolbox after changing"},

  // Package
  {"pkg.installer", "软件包安装器", "Package Installer"},
  {"pkg.add_content", "附加内容管理器", "Add-on Content Manager"},

  // Payloads / kstuff
  {"payloads.link", "Payload", "Payload"},
  {"kstuff.group", "Kstuff", "Kstuff"},
  {"kstuff.group.sub", "内核补丁组件管理", "Kernel patch component management"},
  {"kstuff.autoload", "OnionHEN 启动时自动加载 Kstuff", "Auto-load Kstuff when OnionHEN starts"},
  {"kstuff.delete", "删除外部 Kstuff（/data/OnionHEN/kstuff.elf）",
   "Remove external Kstuff (/data/OnionHEN/kstuff.elf)"},
  {"kstuff.delete.desc", "将切换回 OnionHEN 内置的 kstuff", "Falls back to the built-in kstuff"},

  // Game
  {"cheats.link", "金手指", "Cheats"},
  {"remote_play.link", "远程游玩", "Remote Play"},
  {"game_opts.toggle", "OnionHEN 游戏选项", "OnionHEN game options"},
  {"game_opts.toggle.sub",
   "在游戏选项菜单中显示 OnionHEN 相关选项（金手指等）",
   "Show OnionHEN options in the game menu (cheats, etc.)"},
  {"overlay.group", "游戏覆盖层", "Game Overlay"},
  {"overlay.group.sub", "贴边全宽横条 + 半透明黑底",
   "Edge-to-edge bar + translucent black"},
  {"overlay.pos", "监控条位置", "Monitor bar position"},
  {"overlay.pos.sub", "贴屏幕边缘、宽度 100%；指标居中：CPU · GPU · RAM · IP",
   "Full width at screen edge; metrics centered: CPU · GPU · RAM · IP"},
  {"overlay.pos.top", "顶部贴边", "Top edge"},
  {"overlay.pos.bottom", "底部贴边", "Bottom edge"},
  {"overlay.gpu", "GPU", "GPU"},
  {"overlay.gpu.desc", "显示 GPU 温度与显存占用", "Show GPU temperature and VRAM usage"},
  {"overlay.cpu", "CPU", "CPU"},
  {"overlay.cpu.desc", "显示 CPU 温度与平均使用率", "Show CPU temperature and average usage"},
  {"overlay.cpu_all", "显示全部 CPU 核心使用率", "Show all CPU core usage"},
  {"overlay.cpu_all.desc", "CPU 段改为 8 核分别显示（监控条会加宽）",
   "Show all 8 cores (widens the monitor bar)"},
  {"overlay.ram", "内存", "Memory"},
  {"overlay.ram.desc", "显示系统内存占用", "Show system memory usage"},
  {"overlay.ip", "IP 地址", "IP address"},
  {"overlay.ip.desc", "显示主机局域网 IP", "Show console LAN IP"},

  // System
  {"disp_tids", "在主菜单显示 Title ID", "Show Title ID on home menu"},
  {"disp_tids.sub", "零售机可用，但仅在工具箱激活时显示",
   "Works on retail; only while the toolbox is active"},
  {"fan.group", "风扇控制", "Fan control"},
  {"fan.enable", "启用手动风扇阈值", "Enable manual fan threshold"},
  {"fan.threshold", "调整风扇阈值", "Adjust fan threshold"},
  {"fan.threshold.sub", "按摄氏度调整风扇阈值", "Threshold in degrees Celsius"},
  {"rest.group", "休息模式", "Rest Mode"},
  {"rest.group.sub", "提升休息模式稳定性", "Improve Rest Mode stability"},
  {"rest.delay", "延迟工具箱激活（秒）", "Delay toolbox activation (seconds)"},
  {"rest.delay.sub",
   "延迟工具箱内的补丁以防止卡死（在已有内置延迟之外额外增加）",
   "Extra delay before toolbox patches to avoid freezes (added to built-in delay)"},
  {"rest.kill_util", "进入休息模式时自动关闭 OnionHEN 服务守护进程",
   "Stop OnionHEN utility daemon on Rest Mode entry"},
  {"rest.kill_util.sub", "从休息模式恢复后将重新启动守护进程",
   "Daemon restarts when resuming from Rest Mode"},
  {"rest.kill_game", "进入休息模式时自动关闭已打开的游戏",
   "Close open games on Rest Mode entry"},
  {"rest.kill_game.sub", "进入休息模式时尝试关闭任何已打开的游戏",
   "Attempt to close any open game when entering Rest Mode"},
  {"hdd.external", "外接硬盘", "External HDD"},
  {"license.bd", "蓝光（许可证）激活", "Blu-ray (license) activation"},

  // Shortcuts
  {"sc.cheats", "打开金手指菜单", "Open cheats menu"},
  {"sc.cheats.sub", "从任意位置（含游戏内）打开金手指菜单",
   "Open cheats from anywhere (including in-game)"},
  {"sc.toolbox", "打开 OnionHEN 工具箱", "Open OnionHEN Toolbox"},
  {"sc.toolbox.sub", "从任意位置（含游戏内）打开工具箱",
   "Open the toolbox from anywhere (including in-game)"},
  {"sc.off", "关闭（无快捷键）", "Off (no shortcut)"},
  {"sc.r3_l3", "按住 R3 + L3", "Hold R3 + L3"},
  {"sc.l2_tri", "按住 L2 + △", "Hold L2 + △"},
  {"sc.long_options", "长按选项键", "Long-press Options"},
  {"sc.long_share", "长按分享键", "Long-press Share"},
  {"sc.share", "单击分享键", "Tap Share"},
  {"sc.l2_r3", "按住 L2 + R3", "Hold L2 + R3"},

  // Debug
  {"debug.jb", "应用越狱通知", "App jailbreak notifications"},
  {"debug.jb.sub", "在越狱应用时显示通知", "Show a notification when jailing an app"},
  {"debug.np_env", "NP 环境", "NP environment"},
  {"debug.np_env.confirm", "系统将重启以应用此设置。", "The system will reboot to apply this setting."},
  {"debug.np_env.confirm_phrase", "确定,取消", "OK,Cancel"},

  // Language
  {"lang.list", "工具箱语言", "Toolbox language"},
  {"lang.list.sub", "切换后请退出并重新打开工具箱页面",
   "Leave and re-open the toolbox page after switching"},
  {"lang.system", "跟随系统语言", "System language"},
  {"lang.zh", "简体中文", "简体中文"},
  {"lang.en", "English", "English"},

  // About (UI chrome; person/project names stay as literals in XML)
  {"about.donate", "支持本项目", "Support this project"},
  {"about.donate.sub", "喜欢这个项目吗？欢迎捐赠支持上游作者",
   "Like the project? Consider supporting the upstream authors"},
  {"about.donate.methods", "★ 捐赠方式", "★ Donation methods"},
  {"about.credits", "OnionHEN 致谢", "OnionHEN Credits"},
  {"about.credits.sub", "传承、测试与致谢", "Lineage, testers, and credits"},
  {"about.lineage", "★ 传承 ★", "★ Lineage ★"},
  {"about.lineage.etahen", "- etaHEN (LightningMods) — OnionHEN 源码基线",
   "- etaHEN (LightningMods) — source base of OnionHEN"},
  {"about.lineage.goldhen", "- GoldHEN (SiSTR0) — PS4 AIO HEN 精神前身",
   "- GoldHEN (SiSTR0) — spiritual PS4 AIO HEN predecessor"},
  {"about.testers", "★ 致谢测试人员 ★", "★ Thanks to testers ★"},
  {"about.testers.intro",
   "感谢为项目做出倾心奉献的每一位测试人员。是你们的参与和支持，让项目不断完善与成长。",
   "Thanks to every tester who helped this project grow."},
  {"about.more", "更多信息与更新请关注项目发布渠道",
   "For more info and updates, follow the project release channels"},
  {"about.projects", "OnionHEN 所包含的项目", "Projects included in OnionHEN"},
  {"about.projects.sub", "本仓库实际使用或嵌入的组件",
   "Components actually used or embedded in this tree"},
  {"about.projects.info", "★ 本仓库依赖与嵌入组件",
   "★ Dependencies and embedded components in this tree"},

  // Dynamic pages — payloads
  {"payload.title", "Payload", "Payload"},
  {"payload.auto_title", "★ Payload 自动启动", "★ Payload auto-start"},
  {"payload.auto.link", "★ Payload 自动启动", "★ Payload auto-start"},
  {"payload.auto.sub", "配置在加载 OnionHEN 时自动启动的 .elf（放在 payloads/）",
   "Configure .elf files in payloads/ to start with OnionHEN"},
  {"payload.start_stop", "启动/停止 ", "Start/stop "},
  {"payload.path", " (路径: ", " (path: "},
  {"payload.autostart_enable", "启用/禁用 ", "Enable/disable "},
  {"payload.autostart_suffix", " 的自动启动  (", " auto-start  ("},

  // Cheats
  {"cheats.none", "OnionHEN 金手指 - 当前没有打开的游戏",
   "OnionHEN Cheats - no game is open"},
  {"cheats.none.hint", "请先启动游戏后再打开金手指菜单",
   "Launch a game first, then open the cheats menu"},
  {"cheats.missing", "未找到此游戏/版本的金手指文件（请手动放到 /data/OnionHEN/cheats/）",
   "No cheats for this game/version (place files under /data/OnionHEN/cheats/)"},
  {"cheats.title_prefix", "OnionHEN 金手指 - ", "OnionHEN Cheats - "},
  {"cheats.ver_unknown", "无法检测补丁版本", "Could not detect patch version"},
  {"cheats.not_running", " 当前未运行，除非打开游戏否则无法激活任何金手指",
   " is not running; open the game to activate cheats"},
  {"cheats.authors", "金手指作者: ", "Cheat authors: "},
  {"cheats.on_off", "开/关", "On/Off"},
  {"cheats.enable_for", "为 ", "Enable/disable "},
  {"cheats.enable_mid", " 启用/禁用 ", " for "},

  // Remote play
  {"rp.title", "远程游玩连接详情", "Remote Play connection details"},
  {"rp.need_reboot", "账号已由 OnionHEN 激活，请重启主机后再使用远程游玩！",
   "Account was activated by OnionHEN; reboot the console before using Remote Play!"},
  {"rp.account_id", "账号 ID: ", "Account ID: "},
  {"rp.account_id_decoded", "解码后账号 ID: ", "Decoded account ID: "},
  {"rp.pin", "PIN 码  : ", "PIN code: "},
  {"rp.save_usb", "将远程游玩详情保存到 USB", "Save Remote Play details to USB"},

  // Plapps
  {"plapps.title", "OnionHEN Payload 自制软件 - 应用程序",
   "OnionHEN Payload homebrew - Applications"},
  {"plapps.version", " | 版本: ", " | version: "},
};
// clang-format on

const Entry *find_entry(const char *key) {
  if (!key)
    return nullptr;
  for (const Entry &e : kTable) {
    if (std::strcmp(e.key, key) == 0)
      return &e;
  }
  return nullptr;
}

Lang lang_from_ui_value(int ui_lang) {
  return ui_lang == 2 ? Lang::En : Lang::ZhHans;
}

bool system_lang(Lang &out) {
#ifdef ONION_HOST_TEST
  (void)out;
  return false;
#else
  constexpr int kSystemServiceParamIdLang = 1;
  constexpr int kSystemParamLangChineseTraditional = 10;
  constexpr int kSystemParamLangChineseSimplified = 11;

  int language = -1;
  if (!sceSystemServiceParamGetInt ||
      sceSystemServiceParamGetInt(kSystemServiceParamIdLang, &language) < 0) {
    return false;
  }

  out = language == kSystemParamLangChineseTraditional ||
                language == kSystemParamLangChineseSimplified
            ? Lang::ZhHans
            : Lang::En;
  return true;
#endif
}

} // namespace

Lang active_lang() { return g_lang; }

int active_ui_lang_value() { return g_lang == Lang::En ? 2 : 1; }

void set_lang(Lang lang) {
  if (lang != Lang::ZhHans && lang != Lang::En)
    lang = Lang::ZhHans;
  g_lang = lang;
}

void apply_ui_lang(int ui_lang) {
  set_lang(lang_from_ui_value(ui_lang));
}

void apply_system_or_ui_lang(int ui_lang) {
  Lang lang = Lang::ZhHans;
  if (ui_lang == 0 && system_lang(lang)) {
    set_lang(lang);
    return;
  }
  set_lang(lang_from_ui_value(ui_lang));
}

const char *tr(const char *key) {
  const Entry *e = find_entry(key);
  if (!e)
    return key ? key : "";
  return g_lang == Lang::En ? e->en : e->zh;
}

} // namespace toolbox_i18n
