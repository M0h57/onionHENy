# 金手指 C++ 架构

## 目标

把 util 金手指收成可扩展的 C++ 模块：编排、写内存策略、格式解析策略用 C++ 表达；ShnExt 重型 crypto/deflate/keystone 仍为 C 翻译单元，经 **Adapter** 接入。

## 设计模式（克制使用）

| 模式 | 用在哪 | 解决什么 |
|------|--------|----------|
| **Facade** | `CheatService` | IPC 只依赖一个入口：export / toggle / flatten / ensureDir |
| **Strategy** | `IMemoryBackend` | mdbg vs kdirect 可替换写内存策略 |
| **Factory Method** | `MemoryBackendFactory::create()` | 按 `util_system_fw_major()` 选后端 |
| **Strategy + Factory** | `ICheatParser` / `CheatParserFactory` | json / shn / mc4 / ShnExt 解析路径统一 |
| **Adapter** | `ShnExtCheatParser` | 包装 `orion_cheat_parse_shnext_buffer` |
| **RAII** | `std::mutex` + `lock_guard` | 锁与服务状态生命周期 |
| **Singleton（进程级服务）** | `CheatService::instance()` | 与原全局 service 同生命周期，线程安全访问 |

不引入：Observer、Command、Abstract Factory 全家桶（当前无多套产品族需求）。

## 模块结构

```text
orion::cheats
├── IMemoryBackend.hpp
│   ├── MdbgMemoryBackend
│   └── KdirectMemoryBackend
├── MemoryBackendFactory
├── ICheatParser.hpp
│   ├── JsonCheatParser
│   ├── XmlCheatParser          # .shn Trainer XML
│   ├── Mc4CheatParser          # AES-256-CBC → XML
│   └── ShnExtCheatParser       # Adapter → C parser
├── CheatParserFactory          # createByFormat / loadFile / loadBuffer
├── CheatApplier                # toggle / master-code / verify / code-cave
├── CheatRepository             # 路径解析、热重载签名、调用 Factory
├── CheatFlatten                # 仓库 flatten 安装（C）
└── CheatService                # Facade + 互斥状态
        │
        ▼
   C: cheat_engine_utils / ShnExt / third_party crypto
        │
        ▼
   util_platform + pt/mdbg/kernel
```

## 调用顺序

```text
IPC handleIPC
  → CheatService::instance()
       → exportList / toggle
            → Repository::refresh (path + file signature)
            → CheatParserFactory::loadFile
                 → ICheatParser::parse (by extension)
            → Applier::toggle
                 → MemoryBackendFactory
                 → util_find_module
                 → backend read/write (+ optional code cave)
```

## 格式解析

| 扩展名 | Parser | 说明 |
|--------|--------|------|
| `.json` | `JsonCheatParser` | GoldHEN/OrionHEN JSON（手写 key 扫描） |
| `.shn` | `XmlCheatParser` | Trainer XML |
| `.mc4` | `Mc4CheatParser` | Base64 + AES-256-CBC → XML |
| `.ShnExt` | `ShnExtCheatParser` | deflate + AES + cJSON + 可选 keystone |

加载入口：`CheatParserFactory::loadFile` / `loadBuffer`（`CheatRepository` 调用）。

## 扩展点

- **新写内存后端**：实现 `IMemoryBackend`，在 `MemoryBackendFactory` 注册固件条件。
- **新文件格式**：实现 `ICheatParser`，在 `CheatParserFactory::createByFormat` 注册扩展名；Repository 路径扫描列表同步扩展。
- **新 IPC 操作**：只扩 `CheatService` 公开方法，避免 IPC 直接碰 Applier/Backend。
