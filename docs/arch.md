# OrionHEN 架构分析

OrionHEN 是 PS5 的 All-in-One Homebrew Enabler，基于 **etaHEN**（LightningMods）GPLv3 源码的社区续作，定位类似 PS4 上的 GoldHEN。

| 维度 | 说明 |
|------|------|
| 目标平台 | PS5（Prospero），目标三元组 `x86_64-sie-ps5` |
| 定位 | 内核 exploit 之后加载的 **payload 套件**：提权、服务、Toolbox UI、fPKG/fSELF 支持等 |
| 许可证 | GPLv3 |
| 构建 | CMake + Ninja + `PS5_PAYLOAD_SDK`（clang） |

**运行时前置条件：** 必须先有 **外部 elfldr（端口 9021）**。OrionHEN 不再内嵌 9021 服务，只通过它启动子 ELF。

---

## 1. 总体架构

### 1.1 加载链路

```
[WebKit / IPV6 等 Kernel Exploit]
        │
        ▼
  OrionHEN.elf  (unpacker)
        │  LZMA 解压内嵌 bootstrapper
        │  经 9021 送出 bootstrapper.elf
        ▼
  bootstrapper.elf
        │  1. 提权 (elfldr_raise_privileges)
        │  2. remount /system、/system_ex
        │  3. unmount /update（阻止系统更新）
        │  4. 写出 util / daemon / kstuff 到 /data/OrionHEN/daemons/
        │  5. 经 9021 顺序启动：util → kstuff → daemon
        │  6. 加载 /data/OrionHEN/plugins/ 插件
        ▼
┌───────────────┬────────────────┬──────────────────┐
│  util.elf     │  kstuff.elf    │  daemon.elf      │
│  工具服务守护 │  fself / fpkg  │  核心守护进程    │
│  Cheats      │  内核补丁      │  ShellUI 注入    │
└───────┬───────┴───────┬────────┴────────┬─────────┘
        │               │                 │
        │               │                 ▼
        │               │         inject shellui.elf
        │               │         → SceShellUI (Toolbox)
        │               │                 │
        │               └─────────────────┼── 共用 ptrace / ShellUI
        ▼                                 ▼
  Unix IPC + TCP 9028            fps_elf（游戏 overlay）
```

**启动顺序有意串行化：** util → kstuff → daemon。

- util 先提供网络/IPC 等服务
- kstuff 需先完成 ShellUI 补丁
- daemon 再注入 Toolbox

若并行 ptrace 同一进程，容易导致 Toolbox 超时或崩溃。

### 1.2 构建嵌入关系

```
shellui.elf ──┐
fps_elf.elf ──┼──► 嵌入 daemon.elf ──┐
              │                       │
              │     util.elf ─────────┼──► 嵌入 bootstrapper.elf
              │     kstuff.elf ───────┤         │
              │                       │         ▼ LZMA
              └───────────────────────┘   bootstrapper.elf.lzma
                                              │
                                              ▼ 嵌入
                                         OrionHEN.elf  (最终用户 payload)
```

构建阶段（`scripts/build.sh`）：

1. CMake configure
2. 内部库 + `shellui` + `fps_elf`
3. sync vendor（kstuff）
4. `daemon` + `util`
5. `bootstrapper`（再 lzma 压缩）
6. `unpacker` → `OrionHEN.elf`

产物落在 `source/bin/`。

### 1.3 仓库布局

```
OrionHEN/
├── assets/           # 图标等
├── docs/             # 技术文档（含本文）
├── scripts/          # 构建与主机侧工具
├── source/           # CMake 主工程
│   ├── bootstrapper/ # 启动器
│   ├── daemon/       # 核心守护
│   ├── util/         # 工具守护（金手指、IPC 等）
│   ├── shellui/      # Toolbox
│   ├── fps_elf/      # 游戏 overlay
│   ├── unpacker/     # 最终 OrionHEN.elf
│   ├── libhijacker/ libNineS/ libNidResolver/
│   ├── liborion_*    # 共享：ipc/settings/proc/platform/ready/detour/plugin/playtime/elfldr
│   ├── extern/       # 第三方源码
│   ├── include/ lib/ # 公共头文件与预编译库
│   └── vendor/       # 同步后的 kstuff 等
└── third_party/      # git submodules（elfldr、kstuff-lite）
```

---

## 2. 模块职责

### 2.1 `unpacker` → `OrionHEN.elf`

- 用户侧最终 payload
- 内嵌 **LZMA 压缩** 的 `bootstrapper.elf`
- 解压后通过 **9021** 发送并执行
- 依赖 7zip-sdk 的 LZMA 解码实现

### 2.2 `bootstrapper` → `bootstrapper.elf`（再压成 `.lzma`）

- 提权、分区 remount、阻止更新
- 以 `.incbin` 嵌入 `daemon.elf`、`util.elf`、`kstuff.elf` 与图标资源
- 写盘到 `/data/OrionHEN/`，经 9021 拉起子进程
- 扫描并加载插件目录
- 可选日志端口 **9088**

关键启动策略：

1. 先写 util / daemon / kstuff 到磁盘
2. 检查 `127.0.0.1:9021` 上的 elfldr
3. 顺序 `file:` URI 启动：util → kstuff → daemon
4. 加载 `/data/OrionHEN/plugins/` 下插件

可用 `/data/OrionHEN/no_kstuff` 或 `/mnt/usb0/no_kstuff` 跳过 kstuff。

### 2.3 `daemon` → `daemon.elf`（Critical 守护进程）

- 内嵌 `shellui.elf`、`fps_elf.elf`
- 经 **libNineS** 将 Toolbox 注入 `SceShellUI`
- 监视 util，崩溃可重启
- Unix socket：`/system_tmp/OrionHEN_crit_service`
- IPC 前缀 `0x9000000`（`BREW_*`）

主要能力：

- 重挂载、拷贝/删除目录、stat、chmod
- 启用 Toolbox
- 风扇阈值调整
- 强制杀进程 / 杀守护进程

### 2.4 `util` → `util.elf`（Utility 守护进程）

- 与 critical 分离，承载网络/IO 与较重业务，提高稳定性
- Unix socket：`/system_tmp/OrionHEN_util_service`
- IPC 前缀 `0x8000000`（`BREW_UTIL_*`）

| 服务 | 端口 / 入口 | 说明 |
|------|-------------|------|
| Legacy CMD | **9028** | 旧版 hijacker 协议（app jailbreak 等） |
| Cheats | IPC | flat-file cheat engine（flat `TITLE_VERSION.ext` + mdbg/kdirect）；详见 [util_arch](util_arch/) |
| ShellCore / ShellUI 补丁 | — | 休息模式恢复、toolbox 激活等 |

> **已移除：** FTP（1337）、Klog 网络服务（9081）。  
> 注意：代码里仍使用 `ps5/klog.h` 的 `klog_printf` / `klog_puts`，那是内核日志 API，不是 9081 服务。

### 2.5 `shellui` → `shellui.elf`（Toolbox UI）

- 注入 `SceShellUI` 的 Mono 层
- 替换/扩展 Debug Settings 风格菜单
- 菜单 XML 经 `encryptxml.py` 加密为 `.sxml` 后嵌入

主要菜单能力：

- 软件包安装（系统 PkgInstaller UI）
- 插件与内核（插件 ELF、kstuff 管理）
- 游戏功能（金手指引擎、Remote Play、overlay）
- 系统设置（Title ID、风扇、休息模式、外置 HDD、BD 激活）
- 启动与快捷键
- 调试 / 关于

注入路径详见 [shellui-injection.md](shellui-injection.md)。

### 2.6 `fps_elf` → `fps_elf.elf`

- 游戏内 overlay（FPS / CPU / RAM / GPU / IP / kstuff 状态等）
- 通过 IPC 与 util / daemon 通信

### 2.7 内部静态库

| 库 | 作用 |
|----|------|
| **libhijacker** | 进程劫持、kernel R/W、spawner、调试、通知；依赖 NidResolver |
| **liborion_elfldr** | **唯一** ptrace/`pt_*` + inject 侧 `elfldr_load` / `elfldr_payload_args` / `elfldr_raise_privileges`；**authid 不在每条 ptrace 上翻转**（由 inject 入口一次提权） |
| **libNineS** | 进程注入编排（`inject_elf` / stager）；**pt/elfldr 实现来自 liborion_elfldr** |
| **libNidResolver** | PS5 模块 NID 解析（SHA1 等） |
| **liborion_ipc** | **客户端**（injectee 双单例）+ **服务端传输环**（`ipc_server`：listen/accept/loop/reply）；daemon/util/shellui/fps 共用 |
| **liborion_settings** | 统一 `config.ini` schema；各进程以 `orion::Settings g_settings` 为真相源 |
| **liborion_detour** | 共享 Detour + hde64 钩子栈；shellui / fps_elf 共用 |
| **liborion_proc** | 共享 proc/ucred（allproc 遍历、dynlib、authid）+ **sysctl 进程查询**（`find_pid` / `orion_find_pid_ex` / `isProcessAlive`）；daemon / util / shellui / bootstrapper / fps 共用 |
| **liborion_platform** | 平台叶子：`if_exists` / `touch_file` / `rmtree`、`OrionHEN_log`（可配置 tag/路径）、`orion_notify`；修一处全树受益 |
| **liborion_ready** | 跨进程 ready/runtime 标记（`/system_tmp/orion_ready/<name>` + wait/timeout）；替代固定 sleep 与 ad-hoc 文件旗 |
| **orion/lnc.h** | 共享 LNC 启动 ABI（`LncAppParam` / `Flag` / 错误码）；daemon `launcher.hpp` 仅为 shim |
| **libNineS** | ptrace 注入编排；**proc/ucred → liborion_proc**；**pt/elfldr → liborion_elfldr** |

#### Daemon 模块（加深后）

| 模块 | 职责 |
|------|------|
| **msg.cpp** | 仅 `IPC_loop` + transport 胶水 |
| **ipc_handle.cpp** | crit 命令表分发 |
| **daemon_inject.cpp** | toolbox / fps 注入 |
| **daemon_settings.cpp** | LoadSettings + mtime 缓存 |
| **daemon_fs.cpp** | remount / chmod / test_sb / reply / fan / ForceKill / pid 查找 |

#### ShellUI 模块（加深后）

| 模块 | 职责 |
|------|------|
| **ipc.hpp** | 仅 `orion/ipc_client`（**不**拉 HookedFuncs） |
| **shellui_types.hpp** | 枚举 / 插件 / overlay / settings 类型 |
| **HookedFuncs.hpp** | Mono hooks + UI API（include types） |
| **mono_runtime** | Mono 反射 / 属性读写 / 类查找 |
| **toolbox_xml** | `generate_*_xml` 菜单 XML |
| **settings_ui** | `settings_commit` / SaveSettings 等 UI 侧设置 |
| **shellui_notify / shellui_proc** | UI 用 `notify(const char*)` 与进程/USB 辅助 |
| **hook_onpress + onpress_*** | 表驱动 OnPress：`{id → handler}`，按 network / cheats / overlay / system / packages / plugins / misc 拆域 |

#### Ready / runtime flags 协议

| 标记名 | 发布方 | 等待方 / 用途 |
|--------|--------|----------------|
| `util` | util 在 IPC 线程启动后 | bootstrapper 启动 util 之后 |
| `kstuff` | bootstrapper 在 mprotect 成功后 | daemon 注入 toolbox 前 |
| `daemon` | daemon 在 IPC 线程启动后 | bootstrapper 启动 daemon 之后 |
| `toolbox` | shellui 注入完成后 | daemon `cmd_enable_toolbox`（兼容旧路径 `toolbox_online`） |
| `fps_overlay` | shellui（overlay FPS 开） | daemon 游戏循环触发 fps inject（替代 `/system_tmp/fps_enabled`） |
| `util_booted` | util 冷启动完成后 | rest-mode / toolbox 延迟路径（替代 `util_first_boot`） |

#### IPC 分层（加深后）

```
msg.hpp          协议：路径、magic、命令枚举、IPCMessage
ipc_server.*     传输：Unix listen/accept/recv/send + 线程环 + reply
handleIPC (进程) 业务：crit 与 util 各自命令表
ipc_client.*     注入侧客户端
```

#### 配置分层

```
orion::Settings       持久化 schema（双路径 twin：primary + shellui）
orion::SettingsStore  进程内线程安全真相源（mutex + snapshot/store/update）
g_settings            daemon/util：SettingsStore；shellui：Settings（UI 线程）
LoadSettings()        统一 bool 契约：刷新 store；缺文件用默认并成功
mtime 门控            settings_config_newest_mtime — 任一 twin 更新即失效
运行时原子量          仅 util：g_legacy_cmd_server / g_legacy_cmd_server_exit（热路径）
OverlayLayout         仅 shellui：由 overlay_pos 派生的像素坐标
```

### 2.8 主机工具（`scripts/`）

| 脚本 | 用途 |
|------|------|
| `build.sh` | 一键构建流水线 |
| `sync_vendor.sh` | 同步 kstuff 等 vendor |
| `send_elf.py` / `send_payload.ps1` | 网络发送 ELF |
| `launch.py` | IPC 控制应用 |
| `kill_daemon.py` / `daemon_log.py` | 守护进程管理与日志 |
| `ps5_cmake.sh` | Prospero CMake 封装 |
| `pack_bootstrapper.sh` | bootstrapper 尺寸记录 + lzma 打包 |

---

## 3. IPC 与通信模型

```
shellui / fps_elf / homebrew
        │ Unix domain socket
        ├─► /system_tmp/OrionHEN_crit_service  (daemon, 0x9xxxxxxx)
        └─► /system_tmp/OrionHEN_util_service  (util,   0x8xxxxxxx)

homebrew (legacy)
        └─► TCP 127.0.0.1:9028  (HijackerCommand, magic 0xDEADBEEF)
              ACTIVE / LAUNCH / PROCLIST / KILL / JAILBREAK …
```

### 3.1 消息格式

定义见 `source/include/msg.hpp`：

```cpp
struct IPCMessage {
  int magic = 0xDEADBABE;
  enum DaemonCommands cmd;
  int error = 0;
  char msg[0x1000];
};
```

### 3.2 主要命令族

**Critical（daemon，约 `0x9000000`）：**

- `BREW_REMOUNT_FOLDER` / `BREW_STAT_CMD` / `BREW_COPY_*` / `BREW_DELETE_DIR`
- `BREW_CHMOD_DIR` / `BREW_ENABLE_TOOLBOX`
- `BREW_ADJUST_FAN_SPEED`
- `BREW_KILL_DAEMON` / `BREW_FORCE_KILL_PID`

**Util（约 `0x8000000`）：**

- `BREW_UTIL_LAUNCH_PLUGIN`
- `BREW_UTIL_GET_GAME_VER` / `BREW_UTIL_GET_GAME_CHEAT` / `BREW_UTIL_TOGGLE_CHEAT`
- `BREW_UTIL_DOWNLOAD_CHEATS`（`RELOAD_CHEATS` 已移除，热重载靠文件签名）
- `BREW_UTIL_DOWNLOAD_KSTUFF`
- `BREW_UTIL_TOGGLE_LEGACY_CMD_SERVER`
- `BREW_UTIL_SHELLUI_ON_STANDBY`

**已废弃但保留序号（兼容旧客户端）：**

- `BREW_UNUSED_DECRYPT_DIR`（原 DECRYPT_DIR，SELF 目录解密已移除）
- `BREW_UNUSED_TESTKIT_CHECK`（原 TESTKIT_CHECK；客户端改为本地探测）
- `BREW_UTIL_UNUSED_FTP`（原 TOGGLE_FTP）
- `BREW_UTIL_UNUSED_KLOG`（原 TOGGLE_KLOG）
- `BREW_UTIL_LAUNCH_ELFLDR`（9021 服务不再内嵌）

### 3.3 运行时路径

| 路径 | 用途 |
|------|------|
| `/data/OrionHEN/` | 数据根目录 |
| `/data/OrionHEN/config.ini` | 配置 |
| `/data/OrionHEN/OrionHEN.log` | 日志 |
| `/data/OrionHEN/daemons/` | util / daemon / kstuff 写盘位置 |
| `/data/OrionHEN/plugins/` | 插件 |
| `/data/OrionHEN/payloads/` | payload ELF |
| `/system_tmp/OrionHEN_*_service` | Unix IPC socket |
| `/system_tmp/toolbox_online` | Toolbox 注入就绪信号 |

---

## 4. 功能清单

### 4.1 系统 / HEN 核心

- 提权与分区 remount
- 阻止系统更新（unmount `/update`）
- **kstuff**：fself / fpkg 相关内核能力（通常 ≥ 3.00）
- App jailbreak（IPC / 9028）
- 双守护进程架构（util 可被 daemon 拉起）

### 4.2 用户界面（Toolbox）

- Debug Settings 替代菜单
- Rest Mode / Remote Play / 插件与 payload 管理
- 外置 HDD、kstuff 菜单
- 游戏选项、Title ID、手柄快捷键
- Blu-ray 激活、BD-J/Lua exploit 自动弹碟
- 游戏 overlay
- 金手指（flat 文件 + mdbg/kdirect）

### 4.3 网络服务

- Legacy CMD 9028
- 外部 ELF 加载依赖 **9021 elfldr**

### 4.4 扩展

- 自定义插件（兼容 [etaHEN SDK](https://github.com/LightningMods/etaHEN-SDK)）
- `config.ini` 驱动的开关（overlay、快捷键等）

### 4.5 已移除

| 能力 | 说明 |
|------|------|
| 内嵌 9021 elfldr | 运行时自备 |
| FTP 1337 | 服务与 Toolbox 开关已移除 |
| Klog server 9081 | 服务与 Toolbox 开关已移除 |
| ps5debug / app-dumper | 不再内嵌 |
| Byepervisor / hen.bin | 1.xx–2.xx HV 路径不打包 |
| Discord RPC | 已移除 |
| libSelfDecryptor | 已移除；SELF 目录解密 IPC 亦已移除 |
| 沙盒内 `/data` 可见性补丁 | 仅路径可见，非越狱，已移除 |

---

## 5. 依赖组件

### 5.1 构建与运行时（外部）

| 依赖 | 说明 |
|------|------|
| **ps5-payload-sdk** (`PS5_PAYLOAD_SDK`) | Prospero 工具链、`prospero-cmake`、系统头文件 |
| **clang** | 目标 `x86_64-sie-ps5` |
| **elfldr @ 9021** | 运行时必需，不随 OrionHEN 打包 |
| Kernel exploit | 如 IPV6 等，用于先获得代码执行 |

### 5.2 Git Submodules（`third_party/`）

| 组件 | 上游 | 角色 |
|------|------|------|
| **kstuff-lite** | [EchoStretch/kstuff-lite](https://github.com/EchoStretch/kstuff-lite) | 提供 `kstuff.elf`，同步进 bootstrapper |
| **elfldr** | [ps5-payload-dev/elfldr](https://github.com/ps5-payload-dev/elfldr) | 源码参考；不嵌入，运行时自备 |

```bash
git submodule update --init --recursive
./scripts/sync_vendor.sh
```

### 5.3 树内第三方源码（`source/extern/`）

| 库 | 用途 |
|----|------|
| **7zip-sdk (LZMA)** | unpacker 解压 bootstrapper |
| **cJSON** | JSON（通知、IPC 载荷等） |
| **pugixml-1.15** | XML（util 侧） |
| **pfd_sfo_tools** | PFD/SFO 相关工具源码 |
| **cheat engine** | 金手指解析/热重载/内存补丁（JSON/SHN/MC4/ShnExt，mdbg+kdirect；无 KCF/WMDW） |

### 5.4 预编译静态库（`source/lib/*.a`）

| 库 | 典型用途 |
|----|----------|
| **libcurl** + **mbedtls** / **mbedx509** / **mbedcrypto** + **wolfssl** | HTTPS 下载（PKG、cheats、kstuff 等） |
| **libminizip** + **libz** + **libzstd** | 压缩/归档 |
| **libpsl** | Public Suffix List（curl 依赖） |

历史上还留有 `libelfldr.a` / `libelfloader.a` 等；当前 spawn 走 remote 9021（`lib/elfldr_remote.c`）。

### 5.5 PS5 系统库 stub（`source/lib/*.so`）

常见链接目标：

- `libkernel` / `libkernel_sys`
- `SceSystemService` / `SceUserService`
- `SceNet` / `SceNetCtl`
- `ScePad` / `SceNotification` / `SceRegMgr`
- `SceSysmodule` / `SceSysCore` / `SceAppInstUtil`
- `SceHttp2` / `SceSsl`
- `SceVideoOut` / `SceGnmDriver`
- `SceLibcInternal`

运行时解析到主机系统模块。

### 5.6 构建期工具

| 工具 | 用途 |
|------|------|
| **Go stubber**（`source/stubber/`） | NID stub 生成 |
| **Python3** | `encryptxml.py` / `encryptver.py` 处理 Toolbox 资源 |
| **lzma / xz** | bootstrapper 打包 |

---

## 6. 架构特点小结

1. **分层嵌入**  
   用户只下发一个 `OrionHEN.elf`，内部层层解压/拉起全套服务。

2. **双守护进程**  
   critical（注入/监视）与 util（网络/IO）分离，util 可崩溃恢复。

3. **外部 elfldr 边界清晰**  
   spawn 统一走 9021，减小维护面与打包体积。

4. **UI = 进程注入**  
   Toolbox 不是独立 App，而是 ptrace 注入 ShellUI 的 Mono 代码。

5. **kstuff 解耦**  
   fPKG/fSELF 能力来自 kstuff-lite；可用 `/data/OrionHEN/kstuff.elf` 覆盖。

6. **IPC 协议稳定**  
   Unix socket + 兼容旧 9028；废弃命令保留序号，便于旧客户端不崩。

---

## 7. 相关文档

| 文档 | 说明 |
|------|------|
| [../README.md](../README.md) | 项目总览、功能列表、配置、加载方式 |
| [shellui-injection.md](shellui-injection.md) | ShellUI 注入路径与 libNineS 稳定性修复 |
| [pkg-writeup.md](pkg-writeup.md) | PS5 PKG 技术说明 |
| [../source/README.md](../source/README.md) | 源码树与构建说明 |
| [../third_party/README.md](../third_party/README.md) | 子模块与已移除第三方依赖 |

---

## 8. 构建速查

```bash
git submodule update --init --recursive
export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk

./scripts/build.sh
# 或缺 vendor 时仅编译验证：
# ./scripts/build.sh --stub-missing
```

手动 CMake：

```bash
./scripts/ps5_cmake.sh -S source -B source/build -G Ninja -DV_FW=0x3000000
cmake --build source/build
```
