/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include <orion/platform.h>
#include <orion/proc_query.h>
// Avoid common_utils.h here (stdatomic vs C++ atomic clash).
extern "C" void OrionHEN_log(const char *fmt, ...);
pid_t find_pid(const char *name);
#include "hijacker/hijacker.hpp"
#include "dbg/dbg.hpp"
#include <sys/sysctl.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

extern uint64_t shellcore_offset_patch;
extern pid_t g_ShellCorePid;
extern "C" const uint8_t hex_lut[];
uint32_t getSystemSwVersion();
extern "C" int sceKernelGetProcessName(int pid, char *name);

int get_shellcore_pid() {
    /*
     * Prefer process name (ki_comm @ ~0x1BF), not thr name (ki_tdname @ 447).
     * Local find_pid() matches thr name only and often misses SceShellCore
     * → false "SceShellCore not found" / allow_data sandbox patch skipped.
     */
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    size_t buf_size = 0;
    uint8_t *buf = nullptr;
    int pid = -1;

    if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) || !buf_size)
        return -1;
    buf = (uint8_t *)malloc(buf_size);
    if (!buf)
        return -1;
    if (sysctl(mib, 4, buf, &buf_size, NULL, 0)) {
        free(buf);
        return -1;
    }

    for (uint8_t *ptr = buf; ptr < buf + buf_size;) {
        int ki_structsize = *(int *)ptr;
        if (ki_structsize <= 0)
            break;
        pid_t ki_pid = *(pid_t *)&ptr[72];
        /* FreeBSD/PS5 kinfo_proc: ki_comm is process name */
        char *ki_comm = (char *)&ptr[0x1BF];
        if (ki_structsize > 0x1BF + 5 && ki_comm[0] &&
            strstr(ki_comm, "SceShellCore") != NULL) {
            pid = ki_pid;
            break;
        }
        /* also try thr name (some builds only set this) */
        char *ki_tdname = (char *)&ptr[447];
        if (ki_structsize > 447 + 5 && ki_tdname[0] &&
            strstr(ki_tdname, "SceShellCore") != NULL) {
            pid = ki_pid;
            break;
        }
        ptr += ki_structsize;
    }
    free(buf);

    if (pid < 0) {
        for (int j = 1; j < 20000; j++) {
            char tmp_buf[256];
            memset(tmp_buf, 0, sizeof(tmp_buf));
            if (sceKernelGetProcessName(j, tmp_buf) != 0)
                continue;
            if (strcmp(tmp_buf, "SceShellCore") == 0) {
                pid = j;
                break;
            }
        }
    }
    if (pid < 0)
        pid = find_pid("SceShellCore");
    return pid;
}
// Pattern scanning and memory functions
static uint32_t pattern_to_byte(const char *pattern, uint8_t *bytes) {
    uint32_t count = 0;
    const char *start = pattern;
    const char *end = pattern + strlen(pattern);

    for (const char *current = start; current < end; ++current) {
        if (*current == '?') {
            ++current;
            if (*current == '?') {
                ++current;
            }
            bytes[count++] = -1;
        } else {
            bytes[count++] = strtoul(current, (char **)&current, 16);
        }
    }
    return count;
}

__attribute__((noinline)) static uint8_t* hexstrtochar2(const char* hexstr, size_t* size) {
    if (!hexstr || *hexstr == '\0' || !size || *size < 0) {
        return nullptr;
    }

    uint32_t str_len = strlen(hexstr);
    size_t data_len = ((str_len + 1) / 2) * sizeof(uint8_t);
    *size = (str_len) * sizeof(uint8_t);
    uint8_t* data = (uint8_t*)malloc(*size);

    if (!data) {
        return nullptr;
    }

    uint32_t j = 0; // hexstr position
    uint32_t i = 0; // data position

    if (str_len % 2 == 1) {
        data[i] = (uint8_t)(hex_lut[0] << 4) | hex_lut[(uint8_t)hexstr[j]];
        j = ++i;
    }

    for (; j < str_len; j += 2, i++) {
        data[i] = (uint8_t)(hex_lut[(uint8_t)hexstr[j]] << 4) |
            hex_lut[(uint8_t)hexstr[j + 1]];
    }

    *size = data_len;
    return data;
}

void write_bytes32(pid_t pid, uint64_t addr, const uint32_t val) {
    OrionHEN_log("addr: 0x%lx", addr);
    OrionHEN_log("val: 0x%08x", val);
    dbg::write(pid, addr, (void*)&val, sizeof(uint32_t));
}

void write_bytes(pid_t pid, uint64_t addr, const char* hexString) {
    uint8_t* byteArray = nullptr;
    size_t bytesize = 0;
    byteArray = hexstrtochar2(hexString, &bytesize);

    if (!byteArray) {
        return;
    }

    OrionHEN_log("addr: 0x%lx", addr);
    dbg::write(pid, addr, byteArray, bytesize);

    dbg::read(pid, addr, byteArray, bytesize);
    if (byteArray) {
        OrionHEN_log("freeing byteArray at 0x%p", byteArray);
        free(byteArray);
    }
}
uint8_t *PatternScan(const uint64_t module_base, const uint64_t module_size, const char *signature) {
    OrionHEN_log("module_base: 0x%lx module_size: 0x%lx", module_base, module_size);
    if (!module_base || !module_size) {
        return nullptr;
    }

    uint8_t patternBytes[256];
    (void)memset(patternBytes, 0, 256);
    int32_t patternLength = pattern_to_byte(signature, patternBytes);
    
    if (patternLength <= 0 || patternLength >= 256) {
        OrionHEN_log("Pattern length too large or invalid! %i (0x%08x)", patternLength, patternLength);
        OrionHEN_log("Input Pattern %s", signature);
        return nullptr;
    }
    
    uint8_t *scanBytes = (uint8_t *)module_base;
    for (uint64_t i = 0; i < module_size; ++i) {
        bool found = true;
        for (int32_t j = 0; j < patternLength; ++j) {
            if (scanBytes[i + j] != patternBytes[j] && patternBytes[j] != 0xff) {
                found = false;
                break;
            }
        }
        if (found) {
            OrionHEN_log("found pattern at 0x%p", &scanBytes[i]);
            return &scanBytes[i];
        }
    }
    
    return nullptr;
}
// Shell patch functions
// OrionHEN: sandbox /data mount patch disabled (see main.cpp). Kept as stub so
// any leftover callers link; does not touch SceShellCore.
bool patchShellCore() {
    OrionHEN_log("patchShellCore: disabled (Allow_data_in_sandbox not used)");
    return false;
#if 0 /* historical: force /data into app sandboxes via SceShellCore patches */
    const UniquePtr<Hijacker> executable = Hijacker::getHijacker(get_shellcore_pid());
    uintptr_t shellcore_base = 0;
    uint64_t shellcore_size = 0;

    if (executable) {
        shellcore_base = executable->getEboot()->getTextSection()->start();
        shellcore_size = executable->getEboot()->getTextSection()->sectionLength();
        g_ShellCorePid = executable->getPid();
    }
    else {
        orion_notify(true, "SceShellCore not found");
        return false;
    }

    bool status = false;
    (void)memset(backupShellCoreBytes, 0, sizeof(backupShellCoreBytes));
    shellcore_offset_patch = 0;

    if (!shellcore_base || !shellcore_size) {
        return false;
    }

    OrionHEN_log("allocating 0x%lx bytes", shellcore_size);
    char* shellcore_copy = (char*)malloc(shellcore_size);
    OrionHEN_log("shellcore_copy: 0x%p", shellcore_copy);

    if (!shellcore_copy) {
        OrionHEN_log("shellcore_copy is nullptr");
        return false;
    }

    if (dbg::read(g_ShellCorePid, shellcore_base, shellcore_copy, shellcore_size)) {
        uint8_t* shellcore_offset_data1 = nullptr;
        uint8_t* shellcore_offset_data2 = nullptr;
        uint8_t* patch_checker_offset = 0;

        switch (getSystemSwVersion() & VERSION_MASK) {
        case V200:
        case V220:
        case V225:
        case V226:
        case V230:
        case V250:
        case V270:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ec 00 48 89 9d"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? b1 00 83 f8"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 e4 e0 48 81 ec 00 02 00 00 49"
            );
            break;
        case V300:
        case V310:
        case V320:
        case V321:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? 00 01 ?? 89 ?? 40"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? c5 00 83 f8 01 75 5f"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 e4 e0 48 81 ec 00 02 00 00 49"
            );
            break;
        case V400:
        case V402:
        case V403:
        case V450:
        case V451:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ?? ?? 4c 89 bd ?? ?? ?? ?? 48 89 9d ?? ?? ?? ??"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ?? ?? 83 f8 01 75 ?? 41 80 3c 24 00"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 e4 e0 48 81 ec 00 02 00 00 49"
            );
            break;
        case V500:
        case V502:
        case V510:
        case V550:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? fb 00 85 c0 75 0d e8 ?? ?? fb 00 85 c0 0f 84 47"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? c7 00 83 f8 01 75 5e"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 e4 e0 48 81 ec e0 01 00 00 49"
            );
            break;
        case V600:
        case V602:
        case V650:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ?? 01 4c 89 a5 80"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ?? 00 83 f8 01 75 66"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 e4 e0 48 81 ec e0 01 00 00 49"
            );
            break;
        case V700: case V701: case V720: case V740: case V760: case V761:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ?? 01 4c 89 b5 80"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? d7 00 83 f8 01 0f 85 cd"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 83 e4 e0 48 81 ec e0 01 00 00 49 89 cd"
            );
            break;
        case V800: case V820:
            shellcore_offset_data1 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? ?? 01 85 c0 75 0d e8 ?? ?? ?? 01 85 c0 0f 84 c1"
            );
            shellcore_offset_data2 = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "e8 ?? ?? dc 00 83 f8 01 0f"
            );
            patch_checker_offset = PatternScan(
                (uint64_t)shellcore_copy, shellcore_size,
                "55 48 89 e5 41 57 41 56 41 55 41 54 53 48 81 ec c8 01 00 00 49 89 cd"
            );
            break;
        default:
            OrionHEN_log("Unknown firmware: 0x%08x", getSystemSwVersion());
            break;
        }

        OrionHEN_log("shellcore_offset_data1: 0x%p", shellcore_offset_data1);
        OrionHEN_log("shellcore_offset_data2: 0x%p", shellcore_offset_data2);
        OrionHEN_log("patch_checker_offset: 0x%p", patch_checker_offset);


        // uint64_t addr = shellcore_base +  (uint64_t)0x10C01F0;
        // write_bytes(g_ShellCorePid, addr, "554889E5B8142618805DC3");




        if (shellcore_offset_data1 && shellcore_offset_data2) {
            const uint64_t shellcore_offset_patch1 = shellcore_base +
                ((uint64_t)shellcore_offset_data1 - (uint64_t)shellcore_copy);
            const uint64_t shellcore_offset_patch2 = shellcore_base +
                ((uint64_t)shellcore_offset_data2 - (uint64_t)shellcore_copy);

            write_bytes(g_ShellCorePid, shellcore_offset_patch1, "b801000000");
            write_bytes(g_ShellCorePid, shellcore_offset_patch2, "b801000000");

            OrionHEN_log("Patched shellcore for `/data` mount\n"
                "g_ShellCorePid: 0x%08x\n"
                "mkdir(\"/user/devbin\", 0777): 0x%08x\n"
                "mkdir(\"/user/devlog\", 0777): 0x%08x",
                g_ShellCorePid, mkdir("/user/devbin", 0777),
                mkdir("/user/devlog", 0777));
        }

        if (patch_checker_offset) {
            shellcore_offset_patch = shellcore_base +
                ((uint64_t)patch_checker_offset - (uint64_t)shellcore_copy);
            OrionHEN_log("shellcore_offset_patch: 0x%lx", shellcore_offset_patch);
            write_bytes(g_ShellCorePid, shellcore_offset_patch, "554889E5B8142618805DC3");
        }
    }

    if (shellcore_copy) {
        OrionHEN_log("freeing shellcore_copy from 0x%p", shellcore_copy);
        free(shellcore_copy);
        shellcore_copy = nullptr;
    }

    return status;
#endif /* historical sandbox patch */
}

