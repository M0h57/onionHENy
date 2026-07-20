/* Copyright (C) 2025 OnionHEN / LightningMods */

#include <onion/notify_i18n.h>

#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

typedef struct {
  const char *en;
  const char *zh;
} notify_translation_t;

/*
 * English remains the stable source string so existing notify() call sites can
 * opt in without a parallel API. Keep printf conversion specifications exactly
 * aligned between both columns.
 */
static const notify_translation_t kTranslations[] = {
    {"OnionHEN is starting...", "OnionHEN 正在启动..."},
    {"Welcome to OnionHEN", "欢迎使用 OnionHEN"},
    {"Go to the OnionHEN Toolbox", "前往 OnionHEN 工具箱"},
    {" made by ", " · 作者："},
    {"OnionHEN has been cleaned up.", "OnionHEN 已清理完成。"},
    {"Kstuff '%s' doesn't have ELF header.",
     "Kstuff“%s”没有有效的 ELF 文件头。"},
    {"Loading kstuff from: %s", "正在从以下位置加载 Kstuff：%s"},
    {"Failed to create socket: %s", "创建套接字失败：%s"},
    {"Failed to set socket options: %s", "设置套接字选项失败：%s"},
    {"Failed to bind socket: %s", "绑定套接字失败：%s"},
    {"Failed to listen on socket: %s", "监听套接字失败：%s"},
    {"Failed to accept connection: %s", "接受连接失败：%s"},
    {"Start elfldr on 9021 first, then re-run OnionHEN.",
     "请先在 9021 端口启动 elfldr，然后重新运行 OnionHEN。"},
    {"failed to launch util via elfldr", "通过 elfldr 启动 util 失败"},
    {"Failed to load kstuff, continuing without it",
     "Kstuff 加载失败，将在不加载它的情况下继续"},
    {"Failed to load kstuff via elfldr, continuing",
     "通过 elfldr 加载 Kstuff 失败，将继续运行"},
    {"failed to launch daemon via elfldr", "通过 elfldr 启动 daemon 失败"},
    {"Unable to raise privileges", "无法提升权限"},
    {"failed to mount /system_ex\nif you see this reboot",
     "挂载 /system_ex 失败\n如果看到此消息，请重启主机"},
    {"failed to mount /system\nif you see this reboot",
     "挂载 /system 失败\n如果看到此消息，请重启主机"},
    {"The Fatal error has been successfully resolved\n\nyou have nothing to "
     "worry about",
     "致命错误已成功恢复\n\n无需担心"},
    {"OnionHEN has crashed ...\n\nPlease send "
     "/data/OnionHEN/OnionHEN_crash.log to the PKG-Zone discord: "
     "https://discord.gg/BduZHudWGj",
     "OnionHEN 已崩溃……\n\n请将 /data/OnionHEN/OnionHEN_crash.log "
     "发送到 PKG-Zone Discord：https://discord.gg/BduZHudWGj"},
    {"OnionHEN stack shutdown (util + ShellUI + daemon; kstuff remains)",
     "OnionHEN 组件已关闭（util + ShellUI + daemon；Kstuff 保持运行）"},
    {"Unable to Open Fan Settings!", "无法打开风扇设置！"},
    {"Unable to Set Fan Speed!", "无法设置风扇转速！"},
    {"Failed to get shellui pid", "无法获取 ShellUI PID"},
    {"Failed to inject toolbox", "注入工具箱失败"},
    {"Failed to load the OnionHEN toolbox",
     "加载 OnionHEN 工具箱失败"},
    {"Failed to load the OnionHEN toolbox (timeout, ShellUI left running)",
     "加载 OnionHEN 工具箱失败（超时，ShellUI 仍在运行）"},
    {"SceShellUI restarted while loading the OnionHEN toolbox",
     "加载 OnionHEN 工具箱时 SceShellUI 已重启"},
    {"OnionHEN Utility is not running, restarting...",
     "OnionHEN Utility 未运行，正在重启..."},
    {"OnionHEN Utility services successfully restarted",
     "OnionHEN Utility 服务已成功重启"},
    {"OnionHEN Utility services failed to restart — check elfldr :9020/9021",
     "OnionHEN Utility 服务重启失败——请检查 elfldr :9020/9021"},
    {"Coming out of Rest Mode — re-activating the OnionHEN toolbox...",
     "正在退出休息模式——重新激活 OnionHEN 工具箱..."},
    {"App (PID %i) has been granted a jailbreak",
     "应用（PID %i）已获得越狱权限"},
    {"OnionHEN config created! @ /data/OnionHEN/config.ini",
     "OnionHEN 配置已创建：/data/OnionHEN/config.ini"},
    {"Failed to Read the Settings file", "读取设置文件失败"},
    {"calculateTotalSize failed for %s", "计算 %s 的总大小失败"},
    {"copyFile failed for %s", "复制文件失败：%s"},
    {"copyRecursive failed for %s", "递归复制失败：%s"},
    {"Error parsing JSON", "解析 JSON 失败"},
    {"Invalid path of size %d", "路径长度无效：%d"},
    {"Failed to unmount | error %s", "卸载失败 | 错误：%s"},
    {"remount error: %s\nPath: %s", "重新挂载失败：%s\n路径：%s"},
    {"This command is not used anymore", "此命令已不再使用"},
    {"Invalid fan speed: %d. Must be between 0 and 100.",
     "风扇转速 %d 无效，必须介于 0 和 100 之间。"},
    {"Fan speed adjustment is disabled.", "风扇转速调节已禁用。"},
    {"Fan threshold adjusted to %i°C.", "风扇温度阈值已调整为 %i°C。"},
    {"Failed to adjust fan speed.", "调整风扇转速失败。"},
    {"Reloaded Settings", "设置已重新加载"},
    {"Unknown command 0x%X", "未知命令 0x%X"},
    {"app %s not found", "未找到应用 %s"},
    {"IP Address changed to %s", "IP 地址已变更为 %s"},
    {"OnionHEN utilities daemon has crashed ...\n\nSome OnionHEN features "
     "will be unavailable until you reboot",
     "OnionHEN 工具守护进程已崩溃……\n\n重启前，部分 OnionHEN 功能将不可用"},
    {"OnionHEN utilities daemon has crashed...\n\nAttemping to recover...",
     "OnionHEN 工具守护进程已崩溃……\n\n正在尝试恢复..."},
    {"Failed to get tid", "无法获取 Title ID"},
    {"Failed to get game version", "无法获取游戏版本"},
    {"Failed to read SFO file", "读取 SFO 文件失败"},
    {"Failed to load payload\nPath: %s\nKey: %s",
     "加载 Payload 失败\n路径：%s\n标识：%s"},
    {"Failed to load payload!\nPath: %s", "加载 Payload 失败！\n路径：%s"},
    {"Payload launched\nPath: %s\nKey: %s",
     "Payload 已启动\n路径：%s\n标识：%s"},
    {"No cheats available for %s version %s!",
     "没有适用于 %s 版本 %s 的金手指！"},
    {"Only .elf payloads are supported:\n%s",
     "仅支持 .elf Payload：\n%s"},
    {"Invalid ELF file: %s", "无效的 ELF 文件：%s"},
    {"Invalid ELF name: %s", "无效的 ELF 文件名：%s"},
    {"Loading payload %s ...", "正在加载 Payload %s..."},
    {"Failed to launch payload %s (%s)", "启动 Payload %s 失败（%s）"},
    {"LaunchApp failed with error code: %d", "启动应用失败，错误代码：%d"},
    {"Launching app: %s checking for patches ...",
     "正在启动应用 %s，并检查补丁..."},
    {"LaunchApp returned: %d", "启动应用返回：%d"},
    {"The Game is not running, to activate cheats launch the game first",
     "游戏未运行，请先启动游戏再启用金手指"},
    {"[ERROR] Failed to activate %s\nfailed to find game pid",
     "[错误] 无法启用 %s\n未找到游戏 PID"},
    {"★ %s [%s] ★", "★ %s [%s] ★"},
    {"ON", "开启"},
    {"OFF", "关闭"},
    {"[ERROR] Failed to activate %s", "[错误] 无法启用 %s"},
    {"Kstuff will be loaded on next boot", "下次启动时将加载 Kstuff"},
    {"Kstuff will NOT be loaded on next boot", "下次启动时不会加载 Kstuff"},
    {"The external kstuff has been deleted", "外部 Kstuff 已删除"},
    {"Failed to save Remote Play info, USB not found",
     "保存远程游玩信息失败，未找到 U 盘"},
    {"Failed to open Remote Play info file", "打开远程游玩信息文件失败"},
    {"Remote Play info saved to /mnt%s", "远程游玩信息已保存到 /mnt%s"},
    {"To disable CPU overlay, please disable the All CPU usage option first",
     "要关闭 CPU 监控，请先关闭“全部 CPU 核心使用率”选项"},
    {"To change CPU overlay mode, please enable the CPU overlay first",
     "要更改 CPU 监控模式，请先启用 CPU 监控"},
    {"%s killed", "%s 已终止"},
    {"Failed to create auto start file", "创建自动启动文件失败"},
    {"Manual Fan speed threshold is not enabled",
     "尚未启用手动风扇温度阈值"},
    {"Toolbox and Cheats shortcuts cannot be the same, current selection "
     "will NOT be saved",
     "工具箱和金手指快捷键不能相同，当前选择不会保存"},
    {"Toolbox and Cheats long shortcuts cannot be the same, current "
     "selection will NOT be saved",
     "工具箱和金手指长按快捷键不能相同，当前选择不会保存"},
    {"Cheats and Toolbox shortcuts cannot be the same, current selection "
     "will NOT be saved",
     "金手指和工具箱快捷键不能相同，当前选择不会保存"},
    {"Cheats and Toolbox long shortcuts cannot be the same, current "
     "selection will NOT be saved",
     "金手指和工具箱长按快捷键不能相同，当前选择不会保存"},
    {"Language saved. Leave and re-open the toolbox for it to take effect.",
     "语言已保存。退出并重新打开工具箱后生效。"},
    {"Failed to load assembly", "加载程序集失败"},
    {"Failed to find hook target", "未找到 Hook 目标"},
    {"Failed to install hook", "安装 Hook 失败"},
    {"Failed to get LncUtilWrapper image", "无法获取 LncUtilWrapper 映像"},
    {"Failed to get LayerManager class", "无法获取 LayerManager 类"},
    {"Failed to get FindContainerSceneByPath method",
     "无法获取 FindContainerSceneByPath 方法"},
    {"Exception occurred while calling FindContainerSceneByPath",
     "调用 FindContainerSceneByPath 时发生异常"},
    {"Failed to get Game ContainerScene", "无法获取游戏 ContainerScene"},
    {"Failed to find sceRegMgrGetInt", "未找到 sceRegMgrGetInt"},
    {"Failed to detour KillAppWithReason", "Detour KillAppWithReason 失败"},
    {"Failed to find RNPS decrypt ioctl", "未找到 RNPS 解密 ioctl"},
    {"Failed to detour RNPS decrypt ioctl", "Detour RNPS 解密 ioctl 失败"},
    {"failed to detour BootHelper.Boot", "Detour BootHelper.Boot 失败"},
    {"Failed to detour CaptureScreen", "Detour CaptureScreen 失败"},
    {"Failed to get master address", "无法获取主地址"},
    {"Reloading %s scenes", "正在重新加载 %s 场景"},
    {"Installing package from:\n%s", "正在从以下位置安装软件包：\n%s"},
    {"Installation finished with code: %d", "安装结束，返回代码：%d"},
    {"SCE_REGMGR: unable to get REMOTEPLAY_rp_enable (0x%x)",
     "SCE_REGMGR：无法读取 REMOTEPLAY_rp_enable（0x%x）"},
    {"SCE_REGMGR: unable to set REMOTEPLAY_rp_enable (0x%x)",
     "SCE_REGMGR：无法设置 REMOTEPLAY_rp_enable（0x%x）"},
    {"SCE_REGMGR: unable to verify REMOTEPLAY_rp_enable (0x%x)",
     "SCE_REGMGR：无法验证 REMOTEPLAY_rp_enable（0x%x）"},
    {"sceRemoteplayConfirmDeviceRegist 0x%X pair_stat: %d pair_err: %d",
     "sceRemoteplayConfirmDeviceRegist 0x%X 配对状态：%d 配对错误：%d"},
    {"Remote Play paired! For better stability a reboot is recommended",
     "远程游玩已配对！建议重启主机以获得更好的稳定性"},
};

static atomic_int gLanguage = ATOMIC_VAR_INIT(ONION_NOTIFY_LANG_EN);

void onion_notify_set_language(onion_notify_language_t language) {
  if (language != ONION_NOTIFY_LANG_ZH_HANS &&
      language != ONION_NOTIFY_LANG_EN) {
    language = ONION_NOTIFY_LANG_EN;
  }
  atomic_store_explicit(&gLanguage, language, memory_order_relaxed);
}

onion_notify_language_t onion_notify_get_language(void) {
  return (onion_notify_language_t)atomic_load_explicit(&gLanguage,
                                                        memory_order_relaxed);
}

onion_notify_language_t onion_notify_resolve_language(int ui_language,
                                                       int system_language) {
  if (ui_language == 1) {
    return ONION_NOTIFY_LANG_ZH_HANS;
  }
  if (ui_language == 2) {
    return ONION_NOTIFY_LANG_EN;
  }

  /* PS5 language ids: 10=Traditional Chinese, 11=Simplified Chinese. */
  return system_language == 10 || system_language == 11
             ? ONION_NOTIFY_LANG_ZH_HANS
             : ONION_NOTIFY_LANG_EN;
}

void onion_notify_apply_ui_language(int ui_language, int system_language) {
  onion_notify_set_language(
      onion_notify_resolve_language(ui_language, system_language));
}

const char *onion_notify_tr(const char *english) {
  if (!english || onion_notify_get_language() != ONION_NOTIFY_LANG_ZH_HANS) {
    return english ? english : "";
  }

  for (size_t i = 0; i < sizeof(kTranslations) / sizeof(kTranslations[0]); ++i) {
    if (strcmp(kTranslations[i].en, english) == 0) {
      return kTranslations[i].zh;
    }
  }
  return english;
}
