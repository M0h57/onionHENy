#!/usr/bin/env bash
# OrionHEN one-shot build pipeline
#
# Phases:
#   1) configure (prospero-cmake / PS5 payload SDK)
#   2) build libs + shellui + fps_elf  (shellui/fps land in daemon/assets/)
#   3) stage vendor blobs (elfldr, kstuff)
#   4) build daemon + util
#   5) build bootstrapper  (-> bin/bootstrapper.elf + .lzma)
#   6) build unpacker / OrionHEN.elf   (embeds bootstrapper.elf.lzma)
#
# Usage:
#   export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
#   ./scripts/build.sh              # cleans previous outputs first
#   ./scripts/build.sh --clean      # same as default; kept for compatibility
#   ./scripts/build.sh --fw 0x3000000 --jobs 8
#   ./scripts/build.sh --stub-missing   # compile-only placeholders for missing vendor ELFs
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="${ROOT}/source"
BUILD="${BUILD_DIR:-${SOURCE}/build}"
VENDOR="${ORIONHEN_VENDOR:-${SOURCE}/vendor}"
BIN="${SOURCE}/bin"

PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-${PS5SDK:-}}"
V_FW="${V_FW:-0x3000000}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
JOBS="${JOBS:-}"
CONFIGURE_ONLY=0
STUB_MISSING=0
SKIP_UNPACKER=0
SKIP_VENDOR_SYNC=0
INIT_SUBMODULES=0

# Auto job count
if [[ -z "${JOBS}" ]]; then
  if command -v nproc >/dev/null 2>&1; then
    JOBS="$(nproc)"
  elif command -v sysctl >/dev/null 2>&1; then
    JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  else
    JOBS=4
  fi
fi

log()  { printf '\n\033[1;34m==>\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<EOF
OrionHEN build pipeline

Usage: $(basename "$0") [options]

Options:
  --clean              Accepted for compatibility; builds are cleaned by default
  --configure-only     Only run CMake configure
  --fw <hex>           PS5_FW_VERSION / V_FW (default: ${V_FW})
  --jobs <n>           Parallel build jobs (default: ${JOBS})
  --build-type <t>     Debug|Release (default: ${BUILD_TYPE})
  --build-dir <path>   CMake binary dir (default: source/build)
  --vendor <path>      Vendor blob directory (default: source/vendor)
  --stub-missing       Create tiny placeholder ELFs if vendor blobs missing
                       (links, but NOT for real hardware)
  --skip-unpacker      Stop after bootstrapper (no OrionHEN.elf unpacker)
  --skip-vendor-sync   Do not call scripts/sync_vendor.sh
  --init-submodules    git submodule update --init before sync
  -h, --help           This help

Environment:
  PS5_PAYLOAD_SDK   Path to ps5-payload-sdk (required)
  ORIONHEN_VENDOR   Override vendor directory
  BUILD_DIR         Override build directory
  V_FW              Same as --fw

Third-party (git submodules under third_party/ + release downloads):
  See third_party/README.md and scripts/sync_vendor.sh

  kstuff.elf              <- EchoStretch/kstuff-lite
  third_party/elfldr      <- optional source reference for the external 9021 loader

  Removed: elfldr.elf (9021), ps5debug, app-dumper, Byepervisor/hen, Discord RPC

Built-in outputs (no vendor needed):
  shellui.elf, fps_elf.elf  -> written by CMake into daemon/assets/
  daemon.elf, util.elf      -> source/bin/  (embedded by bootstrapper)
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) shift ;;
    --configure-only) CONFIGURE_ONLY=1; shift ;;
    --fw) V_FW="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --build-type) BUILD_TYPE="$2"; shift 2 ;;
    --build-dir) BUILD="$2"; shift 2 ;;
    --vendor) VENDOR="$2"; shift 2 ;;
    --stub-missing) STUB_MISSING=1; shift ;;
    --skip-unpacker) SKIP_UNPACKER=1; shift ;;
    --skip-vendor-sync) SKIP_VENDOR_SYNC=1; shift ;;
    --init-submodules) INIT_SUBMODULES=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "Unknown option: $1 (see --help)" ;;
  esac
done

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------
[[ -n "${PS5_PAYLOAD_SDK}" ]] || die "PS5_PAYLOAD_SDK is not set"
[[ -d "${PS5_PAYLOAD_SDK}" ]] || die "PS5_PAYLOAD_SDK not a directory: ${PS5_PAYLOAD_SDK}"
[[ -x "${PS5_PAYLOAD_SDK}/bin/prospero-cmake" ]] || die "prospero-cmake not found under SDK"
[[ -d "${SOURCE}" ]] || die "source/ missing at ${SOURCE}"

export PS5_PAYLOAD_SDK
export PATH="${PS5_PAYLOAD_SDK}/bin:${PATH}"
export LLVM_CONFIG="${LLVM_CONFIG:-/opt/homebrew/opt/llvm@18/bin/llvm-config}"

need_cmd() { command -v "$1" >/dev/null 2>&1 || die "missing command: $1"; }
need_cmd cmake
need_cmd ninja
need_cmd python3
# lzma or xz required later for bootstrapper packing
if ! command -v lzma >/dev/null 2>&1 && ! command -v xz >/dev/null 2>&1; then
  die "need lzma or xz (e.g. brew install xz)"
fi

#
# prospero-clang++ needs target libc++ / libc++abi / libunwind in the SDK.
# Incomplete binary installs often only have C stubs; CMake then dies with
#   ld.lld: error: unable to find library -lc++
# Official fix: build SDK from source + ./libcxx.sh, or re-install a full
# release zip that includes target/lib/libc++.a (v0.41+).
#
ensure_sdk_libcxx() {
  local libdir="${PS5_PAYLOAD_SDK}/target/lib"
  local need=0
  for f in libc++.a libc++abi.a libunwind.a; do
    [[ -f "${libdir}/${f}" ]] || need=1
  done
  [[ -d "${PS5_PAYLOAD_SDK}/target/include/c++/v1" ]] || need=1

  if [[ "${need}" -eq 0 ]]; then
    ok "SDK C++ runtime present (libc++ / libc++abi / libunwind)"
    return 0
  fi

  warn "SDK missing C++ runtime under ${libdir}"
  warn "Attempting to fetch from ps5-payload-dev/sdk latest release zip…"

  need_cmd curl
  need_cmd unzip

  local tmp
  tmp="$(mktemp -d "${TMPDIR:-/tmp}/orion-sdk-cxx.XXXXXX")"
  # shellcheck disable=SC2064
  trap "rm -rf '${tmp}'" RETURN

  local zip_url="https://github.com/ps5-payload-dev/sdk/releases/latest/download/ps5-payload-sdk.zip"
  if ! curl -fsSL --retry 3 -o "${tmp}/sdk.zip" "${zip_url}"; then
    die "download failed: ${zip_url}
Install a full SDK, or from SDK source tree run:
  sudo make DESTDIR=${PS5_PAYLOAD_SDK} install
  sudo -E ./libcxx.sh"
  fi

  unzip -q -o "${tmp}/sdk.zip" -d "${tmp}/out"
  local root
  root="$(find "${tmp}/out" -maxdepth 2 -type d -name 'ps5-payload-sdk' | head -1)"
  [[ -n "${root}" ]] || root="${tmp}/out/ps5-payload-sdk"
  [[ -d "${root}/target/lib" ]] || die "unexpected SDK zip layout"

  mkdir -p "${libdir}" "${PS5_PAYLOAD_SDK}/target/include"
  for f in libc++.a libc++abi.a libunwind.a libc++experimental.a; do
    if [[ -f "${root}/target/lib/${f}" ]]; then
      cp -f "${root}/target/lib/${f}" "${libdir}/${f}"
    fi
  done
  if [[ -d "${root}/target/include/c++" ]]; then
    rm -rf "${PS5_PAYLOAD_SDK}/target/include/c++"
    cp -a "${root}/target/include/c++" "${PS5_PAYLOAD_SDK}/target/include/"
  fi

  for f in libc++.a libc++abi.a libunwind.a; do
    [[ -f "${libdir}/${f}" ]] || die "still missing ${libdir}/${f} after merge"
  done
  ok "merged C++ runtime into ${PS5_PAYLOAD_SDK}"
}

CMAKE=("${PS5_PAYLOAD_SDK}/bin/prospero-cmake")

# ---------------------------------------------------------------------------
# .incbin path helpers
#
# Clang/LLVM typically resolves .incbin relative to the including source file.
# Embeds use "assets/..." from daemon/source, util/source, bootstrapper/source
# while real files live in <module>/assets. Symlink once so both layouts work.
# ---------------------------------------------------------------------------
ensure_incbin_links() {
  log "Ensuring .incbin path symlinks"
  mkdir -p \
    "${SOURCE}/daemon/assets" \
    "${SOURCE}/util/assets" \
    "${SOURCE}/bootstrapper/assets" \
    "${BIN}"

  # daemon/source/assets -> ../assets
  if [[ ! -e "${SOURCE}/daemon/source/assets" ]]; then
    ln -sfn ../assets "${SOURCE}/daemon/source/assets"
  fi
  # util/source/assets -> ../assets
  if [[ ! -e "${SOURCE}/util/source/assets" ]]; then
    ln -sfn ../assets "${SOURCE}/util/source/assets"
  fi
  # bootstrapper/source/assets -> ../assets
  if [[ ! -e "${SOURCE}/bootstrapper/source/assets" ]]; then
    ln -sfn ../assets "${SOURCE}/bootstrapper/source/assets"
  fi
  # bootstrapper/source/../../bin already == source/bin (no link needed)
  ok "incbin symlinks ready"
}

# ---------------------------------------------------------------------------
# Vendor staging (submodules + GitHub releases via scripts/sync_vendor.sh)
# ---------------------------------------------------------------------------
stage_vendor() {
  if [[ "${SKIP_VENDOR_SYNC}" -eq 1 ]]; then
    warn "skipping vendor sync (--skip-vendor-sync)"
    return 0
  fi

  log "Syncing third-party embeds (submodules / GitHub releases)"
  local args=()
  [[ "${STUB_MISSING}" -eq 1 ]] && args+=(--stub-missing)
  [[ "${INIT_SUBMODULES}" -eq 1 ]] && args+=(--init-submodules)

  if [[ ! -x "${ROOT}/scripts/sync_vendor.sh" ]]; then
    die "missing ${ROOT}/scripts/sync_vendor.sh"
  fi
  "${ROOT}/scripts/sync_vendor.sh" "${args[@]+"${args[@]}"}"
}

# ---------------------------------------------------------------------------
# Configure
# ---------------------------------------------------------------------------
clean_build_artifacts() {
  log "Cleaning previous build outputs"

  case "${BUILD}" in
    ""|"/"|"/Users"|"/Users/${USER:-}"|"${ROOT}"|"${SOURCE}")
      die "refusing to clean unsafe build dir: ${BUILD}"
      ;;
  esac

  rm -rf "${BUILD}"

  rm -f \
    "${SOURCE}/shellui/assets/OrionHEN_toolbox.sxml" \
    "${SOURCE}/shellui/assets/OrionHEN_Lite.sxml" \
    "${SOURCE}/daemon/assets/shellui.elf" \
    "${SOURCE}/daemon/assets/fps_elf.elf" \
    "${BIN}/daemon.elf" \
    "${BIN}/util.elf" \
    "${BIN}/bootstrapper.elf" \
    "${BIN}/bootstrapper.elf.lzma" \
    "${BIN}/bootstrapper.elf.lzma.size" \
    "${BIN}/test.elf" \
    "${BIN}/OrionHEN.elf"

  rm -f \
    "${SOURCE}/lib/libNidResolver.a" \
    "${SOURCE}/lib/libNineS.a" \
    "${SOURCE}/lib/libhijacker.a" \
    "${SOURCE}/lib/liborion_detour.a" \
    "${SOURCE}/lib/liborion_elfldr.a" \
    "${SOURCE}/lib/liborion_ipc.a" \
    "${SOURCE}/lib/liborion_platform.a" \
    "${SOURCE}/lib/liborion_playtime.a" \
    "${SOURCE}/lib/liborion_plugin.a" \
    "${SOURCE}/lib/liborion_proc.a" \
    "${SOURCE}/lib/liborion_ready.a" \
    "${SOURCE}/lib/liborion_settings.a"

  ok "old build outputs removed"
}

configure() {
  log "Configure (${BUILD_TYPE}, V_FW=${V_FW})"
  mkdir -p "${BUILD}"
  "${CMAKE[@]}" \
    -S "${SOURCE}" \
    -B "${BUILD}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DV_FW="${V_FW}" \
    -DPS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK}"
  ok "configured -> ${BUILD}"
}

build_targets() {
  local targets=("$@")
  log "Build: ${targets[*]}  (-j${JOBS})"
  cmake --build "${BUILD}" -j"${JOBS}" --target "${targets[@]}"
}

# ---------------------------------------------------------------------------
# Main pipeline
# ---------------------------------------------------------------------------
main() {
  log "OrionHEN build"
  echo "  ROOT     = ${ROOT}"
  echo "  SDK      = ${PS5_PAYLOAD_SDK}"
  echo "  BUILD    = ${BUILD}"
  echo "  VENDOR   = ${VENDOR}"
  echo "  V_FW     = ${V_FW}"
  echo "  TYPE     = ${BUILD_TYPE}"
  echo "  JOBS     = ${JOBS}"

  clean_build_artifacts
  ensure_incbin_links
  ensure_sdk_libcxx
  configure

  if [[ "${CONFIGURE_ONLY}" -eq 1 ]]; then
    ok "configure-only done"
    exit 0
  fi

  # Phase 1 — libraries + injectables (CMake writes shellui/fps into daemon/assets)
  log "Phase 1/5: libraries + shellui + fps_elf"
  build_targets \
    NidResolver \
    hijacker \
    NineS \
    shellui \
    fps_elf

  # shellui.elf / fps_elf.elf should now be in daemon/assets
  for f in shellui.elf fps_elf.elf; do
    if [[ -f "${SOURCE}/daemon/assets/${f}" ]]; then
      ok "built ${f}"
    else
      # Sometimes outputs land only under bin/ — normalize
      if [[ -f "${BIN}/${f}" ]]; then
        cp -f "${BIN}/${f}" "${SOURCE}/daemon/assets/${f}"
        ok "copied ${f} from bin/ -> daemon/assets/"
      else
        die "expected ${SOURCE}/daemon/assets/${f} after phase 1"
      fi
    fi
  done

  # Phase 2 — vendor embeds required by daemon/util/bootstrapper
  # (can also run earlier; after phase1 so shellui/fps already filled)
  log "Phase 2/5: stage vendor embeds"
  stage_vendor

  # Phase 3 — daemons
  log "Phase 3/5: daemon + util"
  build_targets daemon util

  for f in daemon.elf util.elf; do
    [[ -f "${BIN}/${f}" ]] || die "missing ${BIN}/${f} after phase 3"
    ok "built ${f}"
  done

  # Phase 4 — bootstrapper (embeds daemon/util + kstuff + assets; post-build lzma)
  log "Phase 4/5: bootstrapper"
  build_targets bootstrapper

  # CMake post-build compresses bootstrapper.elf -> bootstrapper.elf.lzma
  # and writes bootstrapper.elf.lzma.size. If lzma replaced the elf, restore
  # naming expected by unpacker.
  if [[ -f "${BIN}/bootstrapper.elf.lzma" ]]; then
    ok "bootstrapper.elf.lzma ready"
  elif [[ -f "${BIN}/bootstrapper.elf" ]]; then
    warn "lzma not produced by CMake post-build; packing manually"
    local elf="${BIN}/bootstrapper.elf"
    if stat -f%z "${elf}" >/dev/null 2>&1; then
      stat -f%z "${elf}" > "${elf}.lzma.size"
    else
      stat -c%s "${elf}" > "${elf}.lzma.size"
    fi
    if command -v lzma >/dev/null 2>&1; then
      # lzma -9 replaces file with .lzma suffix on some implementations
      cp -f "${elf}" "${elf}.bak"
      lzma -f -9 -k "${elf}" 2>/dev/null || lzma -f -9 "${elf}"
      if [[ -f "${elf}.lzma" ]]; then
        :
      elif [[ -f "${elf}" ]] && file "${elf}" | grep -qi lzma; then
        mv "${elf}" "${elf}.lzma"
        mv "${elf}.bak" "${elf}"
      fi
      [[ -f "${elf}.bak" ]] && mv -f "${elf}.bak" "${elf}" 2>/dev/null || true
    else
      xz -F lzma -f -9 -k -c "${elf}" > "${elf}.lzma"
    fi
    [[ -f "${BIN}/bootstrapper.elf.lzma" ]] || die "failed to produce bootstrapper.elf.lzma"
    ok "manual lzma pack done"
  else
    die "bootstrapper.elf missing in ${BIN}"
  fi

  if [[ ! -f "${BIN}/bootstrapper.elf.lzma.size" ]]; then
    # size file must be original decompressed size (ascii or raw — upstream uses stat text)
    if [[ -f "${BIN}/bootstrapper.elf" ]]; then
      if stat -f%z "${BIN}/bootstrapper.elf" >/dev/null 2>&1; then
        stat -f%z "${BIN}/bootstrapper.elf" > "${BIN}/bootstrapper.elf.lzma.size"
      else
        stat -c%s "${BIN}/bootstrapper.elf" > "${BIN}/bootstrapper.elf.lzma.size"
      fi
    fi
  fi

  if [[ "${SKIP_UNPACKER}" -eq 1 ]]; then
    log "Skip unpacker (--skip-unpacker)"
  else
    # Phase 5 — final payload (OrionHEN.elf embeds lzma bootstrapper)
    log "Phase 5/5: unpacker (OrionHEN.elf)"
    # Target project name is OrionHEN (see unpacker/CMakeLists.txt)
    if cmake --build "${BUILD}" -j"${JOBS}" --target OrionHEN 2>/dev/null; then
      ok "OrionHEN target built"
    else
      build_targets unpacker 2>/dev/null || build_targets OrionHEN
    fi
    if [[ -f "${BIN}/OrionHEN.elf" ]]; then
      ok "final payload: ${BIN}/OrionHEN.elf"
    else
      warn "OrionHEN.elf not found under bin/ — check unpacker target name/output"
      ls -la "${BIN}" || true
    fi
  fi

  log "Build finished"
  echo
  echo "Artifacts in ${BIN}:"
  ls -lah "${BIN}" 2>/dev/null || true
  echo
  echo "Daemon embeds in ${SOURCE}/daemon/assets:"
  ls -lah "${SOURCE}/daemon/assets" 2>/dev/null || true
  echo
  ok "done"
}

main
