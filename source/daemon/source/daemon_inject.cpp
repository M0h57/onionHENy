/* Copyright (C) 2025 OrionHEN / LightningMods */

#include "daemon_ops.hpp"
#include "globalconf.hpp"
#include <orion/platform.h>
#include <orion/proc_query.h>
#include <orion/ready.h>
#include <orion/settings.hpp>
#include <orion/toolbox_timing.h>
#include <orion/fps_shm.h>
#include "globalconf.hpp"
#include "dbg/dbg.hpp"
#include "elf/elf.hpp"
#include "hijacker/hijacker.hpp"
#include <cstring>
#include <unistd.h>
#include <vector>

extern "C" {
int sceKernelMprotect(void *addr, size_t len, int prot);
bool Inject_Toolbox(int pid, uint8_t *elf);
extern uint8_t shellui_elf_start[];
extern const unsigned int shellui_elf_size;
extern uint8_t fps_elf_start[];
extern const unsigned int fps_elf_size;
int _sceApplicationGetAppId(int pid, int *appid);
}

/*
 * NOTE: Do not sceKernelSuspendProcess for FPS inject. GPU CWSR suspend +
 * ptrace hangs the title on the load screen (see cmd_enable_fps_new).
 */
struct GameStuff {
    uintptr_t scePadReadState;
    uintptr_t debugout;
    uintptr_t sceKernelLoadStartModule;
    uintptr_t sceKernelDlsym;
    uintptr_t sceKernelSendNotificationRequest;
    uintptr_t anything;
    uint64_t ASLR_Base = 0;
    char prx_path[256];
    int loaded = 0;

    GameStuff(Hijacker& hijacker) noexcept
        : debugout(hijacker.getLibKernelAddress(nid::sceKernelDebugOutText)),
        sceKernelLoadStartModule(hijacker.getLibKernelAddress(nid::sceKernelLoadStartModule)),
        sceKernelDlsym(hijacker.getLibKernelAddress(nid::sceKernelDlsym)),
        sceKernelSendNotificationRequest(hijacker.getLibKernelAddress(nid::sceKernelSendNotificationRequest)) {
    }
};

struct GameBuilder {

    static constexpr size_t SHELLCODE_SIZE = 218;
    static constexpr size_t EXTRA_STUFF_ADDR_OFFSET = 2;

    uint8_t shellcode[SHELLCODE_SIZE];

    void setExtraStuffAddr(uintptr_t addr) noexcept {
        *reinterpret_cast<uintptr_t*>(shellcode + EXTRA_STUFF_ADDR_OFFSET) = addr;
    }
};

static constexpr GameBuilder BUILDER_TEMPLATE{
    0x48, 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // MOV scePadReadState, RDX // 10


    // Additional shellcode0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x60, 0x4c, 0x8b, 0x62, 0x20,0x55, 0x41, 0x57, 0x41, 0x56, 0x41, 0x54, 0x53, 0x48, 0x83, 0xec, 0x30, 0x4c, 0x8b, 0x62, 0x20,
    0x55, 0x41, 0x57, 0x41, 0x56, 0x53, 0x48, 0x83, 0xec, 0x48, 0x48, 0xb8, 0x73, 0x68, 0x65, 0x6c,
    0x6c, 0x6d, 0x61, 0x69, 0x48, 0xb9, 0x6e, 0x20, 0x69, 0x73, 0x20, 0x6e, 0x75, 0x6c, 0x48, 0xc7,
    0x44, 0x24, 0x08, 0x00, 0x00, 0x00, 0x00, 0x49, 0x89, 0xd6, 0x48, 0x89, 0xf3, 0x89, 0xfd, 0x48,
    0x89, 0x44, 0x24, 0x30, 0x48, 0x89, 0x4c, 0x24, 0x38, 0x48, 0xc7, 0x44, 0x24, 0x40, 0x6c, 0x00,
    0x00, 0x00, 0x48, 0x89, 0x44, 0x24, 0x10, 0x48, 0xc7, 0x44, 0x24, 0x18, 0x6e, 0x00, 0x00, 0x00,
    0x48, 0xc7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00, 0xff, 0x12, 0x41, 0x89, 0xc7, 0x85, 0xed,
    0x7e, 0x60, 0x45, 0x85, 0xff, 0x75, 0x5b, 0x80, 0x7b, 0x4c, 0x00, 0x74, 0x55, 0x41, 0x83, 0xbe,
    0x38, 0x01, 0x00, 0x00, 0x00, 0x75, 0x4b, 0x49, 0x8d, 0x7e, 0x38, 0x31, 0xf6, 0x31, 0xd2, 0x31,
    0xc9, 0x45, 0x31, 0xc0, 0x45, 0x31, 0xc9, 0x41, 0xff, 0x56, 0x10, 0x48, 0x8d, 0x74, 0x24, 0x10,
    0x48, 0x8d, 0x54, 0x24, 0x08, 0x89, 0xc7, 0x41, 0xff, 0x56, 0x18, 0x48, 0x8b, 0x44, 0x24, 0x08,
    0x48, 0x85, 0xc0, 0x74, 0x07, 0x4c, 0x89, 0xf7, 0xff, 0xd0, 0xeb, 0x0b, 0x48, 0x8d, 0x74, 0x24,
    0x30, 0x31, 0xff, 0x41, 0xff, 0x56, 0x08, 0x41, 0xc7, 0x86, 0x38, 0x01, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x44, 0x89, 0xf8, 0x48, 0x83, 0xc4, 0x48, 0x5b, 0x41, 0x5e, 0x41, 0x5f, 0x5d, 0xc3,
};

bool HookGame(UniquePtr<Hijacker>& hijacker, uint64_t alsr_b) {
    OrionHEN_log("Patching Game Now");

    GameBuilder builder = BUILDER_TEMPLATE;
    GameStuff stuff{ *hijacker };

    UniquePtr<SharedLib> lib = hijacker->getLib("libScePad.sprx");
    if (lib.get() == nullptr) {
        OrionHEN_log("libScePad.sprx not found!");
        return false;
    }
    OrionHEN_log("libScePad.sprx addr: 0x%llx", lib->imagebase());
    stuff.scePadReadState = hijacker->getFunctionAddress(lib.get(), nid::scePadReadState);

    //libSceGnmDriver
    UniquePtr<SharedLib> gnmlib = hijacker->getLib("libSceGnmDriverForNeoMode.sprx");
    if (gnmlib.get() == nullptr) {
        OrionHEN_log("libSceGnmDriverForNeoMode.sprx not found!");
        gnmlib = hijacker->getLib("libSceGnmDriver.sprx");
        if (gnmlib.get() == nullptr) {
            OrionHEN_log("libSceGnmDriver.sprx not found!");
			return false;
		}   
    }
    OrionHEN_log("libSceGnmDriver.sprx addr: 0x%llx", gnmlib->imagebase());
    stuff.anything = hijacker->getFunctionAddress(gnmlib.get(), nid::sceGnmSubmitAndFlipCommandBuffersForWorkload);

    OrionHEN_log("scePadReadState addr: 0x%llx", stuff.scePadReadState);
    if (stuff.scePadReadState == 0) {
        OrionHEN_log("failed to locate scePadReadState");
        return false;
    }

    stuff.ASLR_Base = alsr_b;
    strcpy(stuff.prx_path, "/data/OrionHEN/fps.prx");

    auto code = hijacker->getTextAllocator().allocate(GameBuilder::SHELLCODE_SIZE);
    OrionHEN_log("shellcode addr: 0x%llx", code);
    auto stuffAddr = hijacker->getDataAllocator().allocate(sizeof(GameStuff));
    // static constexpr Nid printfNid{"hcuQgD53UxM"};
    // static constexpr Nid amd64_set_fsbaseNid{"3SVaehJvYFk"};
    auto meta = hijacker->getEboot()->getMetaData();
    const auto& plttab = meta->getPltTable();
    auto index = meta->getSymbolTable().getSymbolIndex(nid::scePadReadState);
    for (const auto& plt : plttab) {
        if (ELF64_R_SYM(plt.r_info) == index) {
            builder.setExtraStuffAddr(stuffAddr);
            hijacker->write(code, builder.shellcode);
            hijacker->write(stuffAddr, stuff);

            uintptr_t hook_adr = hijacker->getEboot()->imagebase() + plt.r_offset;

            // write the hook
            hijacker->write<uintptr_t>(hook_adr, code);
            OrionHEN_log("hook addr: 0x%llx", hook_adr);
            hijacker.release();

            return true;
        }
    }
    return false;
}

int done_appid;
extern "C" int sceKernelGetCurrentFanDuty(int *unk, int *duty);

/*
 * SCE appid != process pid. Suspend/resume/ptrace must use the process pid.
 * Prefer the BigApp lookup; fall back to appid→pid scan.
 */
static int resolve_game_pid_for_appid(int appid) {
    int pid = get_game_pid();
    if (pid > 0) {
        int got = 0;
        if (_sceApplicationGetAppId(pid, &got) == 0 && got == appid)
            return pid;
        /* BigApp pid may still be the right target even if appid probe fails. */
        if (got == 0 || got == appid)
            return pid;
    }

    for (int j = 1; j <= 9999; j++) {
        int bappid = 0;
        if (_sceApplicationGetAppId(j, &bappid) < 0)
            continue;
        if (bappid == appid) {
            OrionHEN_log("resolved appid 0x%X -> pid %d", appid, j);
            return j;
        }
    }
    return -1;
}

bool cmd_enable_fps_new(int appid) {
 
    if(done_appid == appid){
       // OrionHEN_log("FPS already enabled for %x", appid);
        return true;
  	}
    
    OrionHEN_log("Enabling fps for appid 0x%X", appid);

    /*
     * Pre-create world-writable sample files. Game sandbox cannot mkdir/create
     * under /data or /system_tmp; fps_elf only opens existing.
     */
    if (orion_fps_shm_ensure() != 0)
      OrionHEN_log("orion_fps_shm_ensure failed — fps_elf may not publish");
    else
      OrionHEN_log("fps SHM files ready for sandboxed writer");

    /*
     * Do NOT sceKernelSuspendProcess here.
     * Suspending the BigApp freezes GPU CWSR and then ptrace attach hangs
     * ("gc_suspend_phase0" then stuck on inject). Soft-inject while the game
     * keeps running; fps_elf defers the GNM detour until load completes.
     */
    sleep(3);

    int pid = resolve_game_pid_for_appid(appid);
    if (pid < 0) {
        orion_notify(true, "Failed to get game pid");
        return false;
    }
    OrionHEN_log("fps inject (no-suspend): appid=0x%X pid=%d", appid, pid);

    if (!Inject_Toolbox(pid, fps_elf_start)) {
        /* Do NOT kill the game — inject failure must not terminate titles. */
        orion_notify(true, "Failed to inject fps (game left running)");
        return false;
    }

    done_appid = appid;
    OrionHEN_log("fps inject done pid=%d (hooks deferred inside fps_elf)", pid);
    return true;
}


bool cmd_enable_fps(int appid) {
   
    if(done_appid == appid){
       // OrionHEN_log("FPS already enabled for %x", appid);
        return true;
	   }

    (void)orion_fps_shm_ensure();

    int pid = resolve_game_pid_for_appid(appid);
    if (pid < 0) {
        OrionHEN_log("cmd_enable_fps: failed to resolve pid for appid 0x%X", appid);
        return false;
    }
    OrionHEN_log("Game is running, appid 0x%X, pid %i (no-suspend HookGame)", appid, pid);

    /*
     * Same policy as fps_new: never SuspendApp for FPS. A failed resume leaves
     * titles permanently stuck on the load screen.
     */
    UniquePtr<Hijacker> executable = Hijacker::getHijacker(pid);
    uintptr_t text_base = 0;
    uint64_t text_size = 0;
    if (executable)
    {
        text_base = executable->getEboot()->getTextSection()->start();
        text_size = executable->getEboot()->getTextSection()->sectionLength();
    }
    else
    {
        OrionHEN_log("Failed to get hijacker for (%d)", pid);
        return false;
    }
    if (text_base == 0 || text_size == 0)
    {
        OrionHEN_log("text_base == 0 || text_size == 0");
        return false;
    }

    for (int attempt = 0; attempt < 30; ++attempt) {
        if (HookGame(executable, text_base))
            break;
        sleep(1);
        if (attempt == 29) {
            OrionHEN_log("HookGame failed for pid %d", pid);
            return false;
        }
    }

    done_appid = appid;
    return true;
}

bool cmd_enable_toolbox(){
    char buz[100] = {0};

    /*
     * If kstuff is present, wait until mprotect works (patches applied) before
     * we ptrace ShellUI. Injecting while kstuff is still patching ShellUI
     * causes "waiting for toolbox" forever / ShellUI crash.
     * (Race seen when daemon+kstuff launched close together via 9021.)
     * Note: we do NOT pause/resume kstuff — other tools may own that; only wait for
     * mprotect readiness.
     */
    if (find_pid("kstuff.elf") > 0 || find_pid("kstuff") > 0) {
      OrionHEN_log("kstuff present — waiting for mprotect before toolbox inject");
      for (int i = 0; i < 20; i++) {
        if (sceKernelMprotect(&buz[0], 100, 0x7) == 0)
          break;
        sleep(1);
      }
      sleep(2);
    }

    OrionHEN_log("Activating toolbox...");
    /*
     * Rest_Mode_Delay only on rest resume — never on cold start.
     * util_booted is true almost immediately after util starts (before this
     * inject), so gating on it alone hung first toolbox load for delay seconds.
     * Rest re-activation delay lives in util patch_checker / check_addr_change.
     */
    LoadSettings();
    {
      const uint64_t delay = g_settings.snapshot().rest_mode_delay_seconds;
      constexpr bool kRestResume = false; /* daemon cold/direct inject path */
      if (orion_toolbox_should_apply_rest_delay(kRestResume, delay)) {
        OrionHEN_log("rest delay %llu (rest resume path)",
                     static_cast<unsigned long long>(delay));
        sleep(static_cast<unsigned int>(delay));
      }
    }

    /* Prefer kstuff ready marker when present; fall back to short settle. */
    if (!orion_ready_wait(ORION_READY_KSTUFF, /*timeout_ms=*/5000, /*poll_ms=*/200)) {
      sleep(1);
    }

    int pid = get_shellui_pid();
    if (pid < 0) {
      orion_notify(true, "Failed to get shellui pid");
      return false;
    }
    OrionHEN_log("Injecting toolbox into SceShellUI pid=%d", pid);

    if (!Inject_Toolbox(pid, shellui_elf_start)) {
      /* Do NOT ForceKill ShellUI — that loops home menu / coredumps */
      orion_notify(true, "Failed to inject toolbox");
      return false;
    }

    /* Ready protocol: shellui signals ORION_READY_TOOLBOX after inject hooks run */
    if (!orion_ready_wait(ORION_READY_TOOLBOX, /*timeout_ms=*/45 * 1000,
                          /*poll_ms=*/250)) {
      orion_notify(true, "Failed to load the OrionHEN toolbox (timeout, ShellUI left running)");
      return false;
    }
    orion_ready_clear(ORION_READY_TOOLBOX);
    OrionHEN_log("Toolbox online (ready protocol)");

    return true;
}

