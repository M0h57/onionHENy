# FPS overlay — known issues

## Status (2026-07-11)

ShellUI PHU-style top bar + SHM plumbing is in place. **In-game GNM flip
detour is disabled** because it freezes titles after the hook arms.

## Confirmed freeze: GNM-WL detour

### What works

| Step | Result |
|------|--------|
| Resolve game **pid** (not appid) | OK |
| Soft inject `fps_elf` (**no** `sceKernelSuspendProcess`) | OK |
| Open SHM (`/system_tmp/onion_fps_shm` etc.) | OK |
| Defer hook 20s after inject | Payload sleeps; process stays alive |

### What freezes the game

Immediately after:

```
DetourFunction: target=0x80a739190 hook=... trampoline=... size=17
[fps] detour OK GNM-WL-link @...
[fps] GNM hook armed — publishing via SHM
```

the title sticks on the load screen / black screen.

### Ruled out

1. **appid vs pid suspend** — `SuspendApp(appid)` was wrong; fixed to resolve
   pid. Correct `SuspendApp(pid)` then made things *worse*: GPU CWSR suspend +
   ptrace hung before inject finished (`gc_suspend_phase0_g2` then stuck on
   elevating/attach). **Never suspend for FPS inject.**
2. **Early boot only** — deferring the detour 20s still freezes at arm time.
   So this is not purely “patch too early”; the **detour itself** is bad for
   this entry (prologue size 17, linked import of
   `sceGnmSubmitAndFlipCommandBuffersForWorkload`).

### Likely causes (not yet proven)

- Detour/hde64 mis-sizes the prologue at the linked GNM import (size=17).
- Patching the process-shared GNM entry breaks other callers / GPU submit path.
- Need a different flip surface (VOut only, game PLT slot, or non-detour
  sampling) instead of the linked `GNM-WL` symbol.

### Current workaround

- `fps_elf` keeps SHM writer + deferred install path, but **does not arm** the
  GNM detour (`ONION_FPS_ARM_GNM_HOOK=0` in `fps_elf/src/prx.cpp`).
- Overlay still reads SHM; ShellUI notify-scrape can still *publish* SHM if a
  notification path provides `FPS …` text.
- Re-enable the detour only after a safe patch strategy is verified on-console.

### Reproduction log (abridged)

```
[eboot.bin] fps_elf (Onion GNM count + SHM)
[fps] shm writer ready fd=32
[fps] payload online shm_fd=32 (hooks deferred 20s)
[fps] deferring GNM hook for 20s ...
… (game still running) …
Hooking 0x80a739190 => ...
InstructionSize: 17
DetourFunction: ... size=17
[fps] detour OK GNM-WL-link ...
[fps] GNM hook armed — publishing via SHM
→ title frozen
```
