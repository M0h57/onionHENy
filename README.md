# OrionHEN

**All-in-one homebrew enabler for the PlayStation 5.**

OrionHEN is a community continuation of **[etaHEN](https://github.com/LightningMods/etaHEN)** by [LightningMods](https://github.com/LightningMods). The original etaHEN project is no longer actively maintained; this repository continues that work under a new name, built **directly on etaHEN’s open-source code** (GPLv3).

```
PS4  →  GoldHEN
PS5  →  etaHEN  →  OrionHEN (this project)
```

---

## Heritage & respect

Homebrew on PlayStation did not appear overnight. OrionHEN stands on the shoulders of two landmark projects:

| Project | Platform | Role |
|---------|----------|------|
| **[GoldHEN](https://github.com/GoldHEN/GoldHEN)** | PS4 | The gold standard AIO HEN for PS4 — the bar every later enabler is measured against |
| **[etaHEN](https://github.com/LightningMods/etaHEN)** | PS5 | The first serious AIO homebrew stack for PS5; **OrionHEN’s direct source base** |

OrionHEN does **not** claim to invent that foundation. We fork, maintain, and extend **etaHEN’s published GPLv3 source**, in the same spirit that GoldHEN defined for the PS4 generation: one payload, many services, practical tools for developers and users.

### Thank you

- **SiSTR0 / GoldHEN team** — for GoldHEN, years of PS4 HEN work, and showing what a polished AIO enabler looks like.
- **LightningMods** — for designing, shipping, and open-sourcing etaHEN under GPLv3 so the community could carry it forward.
- **Every etaHEN & GoldHEN contributor, tester, and reverse engineer** listed below and in their respective projects.

If you benefit from OrionHEN, please also consider supporting the original authors:

- GoldHEN / SiSTR0: [https://ko-fi.com/sistro](https://ko-fi.com/sistro/)
- LightningMods (etaHEN): [GitHub Sponsors](https://github.com/sponsors/LightningMods)

---

## What is OrionHEN?

OrionHEN is a PS5 homebrew enabler and toolbox: debug settings replacement, dual daemons, optional FTP / Klog / elfldr / DPI, Itemzflow integration, kstuff-related flows, plugins, and more — inherited from etaHEN and kept alive here.

- **License:** GPLv3 (same family of obligations as etaHEN; see [`LICENSE`](LICENSE))
- **Source:** [`source/`](source/)
- **Docs:** [`docs/`](docs/)
- **Host tools:** [`scripts/`](scripts/)
- **Prebuilt payload (legacy etaHEN binary packaging, for now):** [`releases/`](releases/)

---

## Repository layout

```
OrionHEN/
├── assets/          # Icons and images
├── docs/            # Technical writeups
├── releases/        # Prebuilt payload binaries
├── scripts/         # Host-side tools (send payload, launch, logs, …)
├── source/          # Full source tree (CMake / Prospero SDK)
├── LICENSE
└── README.md
```

---

## Building from source

Source lives in [`source/`](source/) under GPLv3, with the files required to satisfy the license.

You need a Prospero / PS5 payload SDK (`PS5_PAYLOAD_SDK`) and a clang toolchain targeting `x86_64-sie-ps5`. Build with CMake (see `source/CMakePresets.json`). A thin wrapper is available when the SDK is configured:

```bash
./scripts/ps5_cmake.sh -S source -B source/build
cmake --build source/build
```

Detailed step-by-step build recipes may grow over time; for now, assume a normal CMake workflow on a properly set up SDK.

Technical notes and writeups: [`docs/`](docs/).

---

## Loading the payload

### Exploit sites (community)

- https://tinyurl.com/PS5IPV6 — manual send; often the most stable
- https://ps5jb.pages.dev/ — can auto-load a payload (IPV6 generally preferred over UMTX when available)

### Recommended self-host exploit

- [Modified IPV6 exploit (originally for etaHEN support)](https://github.com/LightningMods/PS5-IPV6-Kernel-Exploit)

### Prebuilt binary

Current tree still ships the last public etaHEN-line binary packaging for convenience while OrionHEN branding and versioning settle in:

- [`releases/etaHEN-2.5B.bin`](releases/etaHEN-2.5B.bin)

Future releases will be published under the **OrionHEN** name.

---

## Host scripts

### Windows PowerShell — `scripts/send_payload.ps1`

Enable scripts if needed:

```powershell
Set-ExecutionPolicy Bypass
```

Or run once:

```powershell
powershell.exe -ExecutionPolicy Bypass -File C:\Path\To\OrionHEN\scripts\send_payload.ps1
```

```powershell
.\scripts\send_payload.ps1 -Payload "C:\path\to\example.elf" -IP "192.168.x.x" -Port XXXX
```

Common ports: exploit elfldr **9020**, SB elfldr **9021**.

### Other tools

| Script | Purpose |
|--------|---------|
| [`scripts/send_elf.py`](scripts/send_elf.py) | Send an ELF over the network |
| [`scripts/launch.py`](scripts/launch.py) | Launch / control apps via IPC |
| [`scripts/kill_daemon.py`](scripts/kill_daemon.py) | Stop the main daemon |
| [`scripts/daemon_log.py`](scripts/daemon_log.py) | Stream daemon logs |
| [`scripts/ps5_cmake.sh`](scripts/ps5_cmake.sh) | Prospero CMake helper |

---

## Features

(Feature set is inherited from etaHEN and will evolve under OrionHEN.)

- ★ Toolbox (debug settings replacement)
- Custom plugins via the [etaHEN SDK](https://github.com/LightningMods/etaHEN-SDK/tree/main/Plugin_samples) (still used until an OrionHEN SDK is split out)
- [Toolbox] Install the Homebrew Store
- [Toolbox] Rest Mode options
- [Toolbox] Remote Play menu
- [Toolbox] Plugin / payload ELF menu with auto-start
- [Toolbox] External HDD menu
- [Toolbox] TestKit menu
- [Toolbox] Kstuff menu
- [Toolbox] Game overlay menu
- [Toolbox] Cheats menu (WIP)
- [Toolbox] Controller shortcuts
- [Toolbox] PS5 webMAN games menu
- [Toolbox] Custom game options menu
- [Toolbox] Display title IDs on home menu
- [Toolbox] Disable toolbox auto-start
- [Toolbox] Blu-ray license activation
- [Toolbox] Disc auto-eject for BD-J / Lua-based exploits
- [Toolbox] Credits / supporters
- [Toolbox] Custom debug settings text and icon
- [Toolbox] Auto-open menu after load
- React bundle (all FWs) & SELF (2.xx) FTP decryption support
- Two daemons for stability (util daemon auto-restarted by the main daemon)
- Custom system software version string
- kstuff-related flows for fself / fpkg support
- Logs under `/data/etaHEN` (path may remain for config compatibility)
- Optional system-wide controller shortcut to open Itemzflow
- Debug settings, game dumper (with Itemzflow), HEN config file
- Jailbreak IPC for homebrew apps
- Update blocker (unmounts update partition)
- Optional Illusions cheats/patches [plugin](https://github.com/LightningMods/etaHEN-SDK/tree/main/Plugin_samples/Illusion_cheats)
- Optional FTP on port **1337**
- Optional `/data` inside app sandboxes
- Klog server on port **9081**
- ELF loader on port **9021** (John’s elfldr)
- Optional PS5Debug
- Itemzflow integration
- Optional Discord RPC on port **8000** ([client setup](https://github.com/jeroendev-one/ps5-rpc-client))
- Optional Direct PKG Installer v2 WebUI: `http://PS5_IP:12800`
- Optional Direct PKG Installer service on port **9090**

### Plugin / SDK

Custom plugins are still developed against the public [etaHEN SDK](https://github.com/LightningMods/etaHEN-SDK). See that repo’s [README](https://github.com/LightningMods/etaHEN-SDK/blob/main/README.md). OrionHEN aims to stay compatible where practical and may publish its own SDK docs later.

### Roadmap (high level)

- Keep the stack building and usable on supported firmwares
- Stability and maintenance after the etaHEN hand-off
- Clear OrionHEN branding for binaries and config over time
- Features previously listed upstream (FPS counter, more userland patches, better PS5 game support via Itemzflow, etc.)

---

## Configuration (`config.ini`)

Settings file: **`/data/etaHEN/config.ini`** (created on first run; reachable via built-in FTP).  
Path may stay under `etaHEN` for a while so existing installs and tools keep working.

| INI key | Description | Default |
|---------|-------------|---------|
| `PS5Debug` | 0 = no PS5Debug (SiSTR0) autoload, 1 = enable | 0 |
| `FTP` | 0 = disable FTP, 1 = enable | 1 |
| `discord_rpc` | 0 = disable Discord RPC, 1 = enable | 0 |
| `toolbox_auto_start` | 0 = off, 1 = on | 1 |
| `Allow_data_in_sandbox` | 0 = no `/data` in app sandbox, 1 = allow | 1 |
| `DPI` | 0 = disable Direct PKG Installer, 1 = enable | 0 |
| `DPI_v2` | 0 = disable DPI v2, 1 = enable | 0 |
| `Klog` | 0 = disable kernel log server, 1 = enable | 0 |
| `ALLOW_FTP_DEV_ACCESS` | 0 = disable FTP dev access, 1 = enable | 0 |
| `StartOption` | 0=None, 1=Home, 2=Settings, 3=Toolbox, 4=Itemzflow | 0 |
| `Rest_Mode_Delay_Seconds` | Delay before shellui patch after rest | 0 |
| `Util_rest_kill` | Kill util daemon on rest | 0 |
| `Game_rest_kill` | Kill open game on rest | 0 |
| `disable_toolbox_auto_start_for_rest_mode` | Disable toolbox autostart after rest | 0 |
| `libhijacker_cheats` | Enable libhijacker cheats | 0 |
| `launch_itemzflow` | Auto-launch Itemzflow | 0 |
| `testkit` | TestKit mode | 0 |
| `Display_tids` | Show title IDs | 0 |
| `APP_JB_Debug_Msg` | App jailbreak debug messages | 0 |
| `etaHEN_Game_Options` | Game options menu | 1 |
| `auto_eject_disc` | Auto eject disc | 0 |
| `Cheats_shortcut_opt` | Cheats shortcut | 0 (`CHEATS_SC_OFF`) |
| `Toolbox_shortcut_opt` | Toolbox shortcut | 0 (`TOOLBOX_SC_OFF`) |
| `Games_shortcut_opt` | Games shortcut | 0 (`GAMES_SC_OFF`) |
| `Kstuff_shortcut_opt` | Kstuff shortcut | 0 (`KSTUFF_SC_OFF`) |
| `overlay_ram` | Overlay: RAM | 0 |
| `overlay_cpu` | Overlay: CPU | 0 |
| `overlay_gpu` | Overlay: GPU | 0 |
| `overlay_ip` | Overlay: IP | 1 |
| `overlay_kstuff` | Overlay: kstuff status | 1 |
| `Overlay_pos` | Overlay position | 0 (`OVERLAY_POS_TOP_LEFT`) |

---

## DPI API (for tool authors)

Direct PKG Installer (port **9090**) is simple and WIP:

1. TCP connect to the PS5 on port **9090**
2. Send JSON: `{ "url" : "http://xxxx" }`
3. Receive: `{ "res" : "0" }` on success
4. Server closes the client socket after the response

---

## Jailbreaking an app (FPKG) via IPC

Requires network + legacy CMD server toolbox settings. Non-whitelist method (protocol as implemented upstream):

```c
enum Commands : int {
  INVALID_CMD = -1,
  ACTIVE_CMD = 0,
  LAUNCH_CMD,
  PROCLIST_CMD,
  KILL_CMD,
  KILL_APP_CMD,
  JAILBREAK_CMD
};

struct HijackerCommand
{
  int magic = 0xDEADBEEF;
  Commands cmd = INVALID_CMD;
  int PID = -1;
  int ret = -1337;
  char msg1[0x500];
  char msg2[0x500];
};

int HJOpenConnectionforBC() {

  SceNetSockaddrIn address;
  address.sin_len = sizeof(address);
  address.sin_family = AF_INET;
  address.sin_port = sceNetHtons(9028); // command server port
  memset(address.sin_zero, 0, sizeof(address.sin_zero));
  sceNetInetPton(AF_INET, "127.0.0.1", &address.sin_addr.s_addr);

  int socket = sceNetSocket("IPC_CMD_SERVER", AF_INET, SOCK_STREAM, 0);
  if (sceNetConnect(socket, (SceNetSockaddr*)&address, sizeof(address)) < 0) {
    close(socket), socket = -1;
  }

  return socket;
}

bool HJJailbreakforBC(int& sock) {

  HijackerCommand cmd;
  cmd.PID = getpid();
  cmd.cmd = JAILBREAK_CMD;

  if (send(sock, (void*)&cmd, sizeof(cmd), MSG_NOSIGNAL) == -1) {
      puts("failed to send command");
      return false;
  }
  else {
    recv(sock, reinterpret_cast<void*>(&cmd), sizeof(cmd), MSG_NOSIGNAL);
    close(sock), sock = -1;
    if (cmd.ret != 0 && cmd.ret != -1337) {
      puts("Jailbreak has failed");
      return false;
    }
    return true;
  }

  return false;
}

int main()
{
     int ret = HJOpenConnectionforBC();
     if (ret < 0) {
         puts("Failed to connect to daemon");
         return -1;
     }
     if (!HJJailbreakforBC(ret))
     {
          puts("Jailbreak failed");
          return -1;
     }

     return 0;
}
```

---

## Credits

### Lineage

- **[GoldHEN](https://github.com/GoldHEN/GoldHEN)** — SiSTR0 and contributors. PS4 AIO HEN; the spiritual predecessor of the “one payload that does everything” model OrionHEN follows on PS5.
- **[etaHEN](https://github.com/LightningMods/etaHEN)** — LightningMods and contributors. **Direct open-source base of OrionHEN.**

### Upstream contributors (etaHEN / shared ecosystem)

- [John Tornblom / PS5-Payload-dev](https://github.com/john-tornblom)
- [Buzzer](https://github.com/buzzer-re)
- [sleirsgoevy](https://github.com/sleirsgoevy)
- [ChendoChap](https://github.com/ChendoChap)
- [astrelsky](https://github.com/astrelsky)
- [illusion](https://github.com/illusion0001)
- CTN & [SiSTR0](https://github.com/SiSTR0) — PS5Debug
- [Nomadic](https://github.com/jeroendev-one) — Discord RPC feature

### Testers (upstream)

- [Echo Stretch](https://twitter.com/StretchEcho)
- [idlesauce](https://github.com/idlesauce)
- [Dizz](https://github.com/DizzRL)
- [BedroZen](https://twitter.com/BedroZen)
- [MODDED WARFARE](https://twitter.com/MODDED_WARFARE)

OrionHEN will add its own contributor and tester lists as this fork grows. If you contributed to etaHEN and want an explicit OrionHEN credit line, open an issue or PR.

---

## License

This project is licensed under the **GNU General Public License v3.0** — see [`LICENSE`](LICENSE).

Because OrionHEN is based on etaHEN’s GPLv3 source, derivative works must remain under compatible terms. We are grateful that LightningMods released etaHEN as free software so the community could continue it.

---

## Disclaimer

OrionHEN is for research and homebrew on devices you own. Use at your own risk. This project is not affiliated with Sony Interactive Entertainment, GoldHEN, or the original etaHEN author beyond use of publicly licensed code and public documentation.
