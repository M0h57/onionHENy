#!/usr/bin/env bash
# Sync / build third-party embeds for OrionHEN.
#
# Remaining embeds: kstuff.elf
# Removed: elfldr.elf (9021 service), ps5debug, ps5-app-dumper, Byepervisor/hen.bin
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE="${ROOT}/source"
TP="${ROOT}/third_party"
VENDOR="${ORIONHEN_VENDOR:-${SOURCE}/vendor}"

PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-}"
FROM_SOURCE=0
STUB_MISSING=0
INIT_SUBMODULES=0

KSTUFF_URL="https://github.com/EchoStretch/kstuff-lite/releases/download/v1.09/kstuff.elf"

log()  { printf '\n\033[1;34m==>\033[0m %s\n' "$*"; }
ok()   { printf '\033[1;32m[ok]\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33m[warn]\033[0m %s\n' "$*"; }
die()  { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

usage() {
  cat <<EOF
Sync OrionHEN vendor embeds from open-source upstreams.

Submodules (under third_party/):
  elfldr           https://github.com/ps5-payload-dev/elfldr          (optional external 9021 loader reference)
  kstuff-lite      https://github.com/EchoStretch/kstuff-lite

Removed from OrionHEN (not synced):
  elfldr.elf (9021 service), ps5debug, ps5-app-dumper, Byepervisor/hen.bin

Options:
  --init-submodules   git submodule update --init --recursive
  --from-source       Prefer building submodules with make (needs SDK)
  --stub-missing      Write tiny placeholders if download/build fails
  -h, --help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --from-source) FROM_SOURCE=1; shift ;;
    --stub-missing) STUB_MISSING=1; shift ;;
    --init-submodules) INIT_SUBMODULES=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unknown option: $1" ;;
  esac
done

download() {
  local url="$1" dest="$2"
  mkdir -p "$(dirname "${dest}")"
  if command -v curl >/dev/null 2>&1; then
    curl -fsSL --retry 3 -o "${dest}.tmp" "${url}"
  elif command -v wget >/dev/null 2>&1; then
    wget -q -O "${dest}.tmp" "${url}"
  else
    return 1
  fi
  mv -f "${dest}.tmp" "${dest}"
}

place() {
  local src="$1" dest="$2"
  mkdir -p "$(dirname "${dest}")"
  cp -f "${src}" "${dest}"
  ok "$(basename "${dest}") <- ${src}"
}

stub() {
  local dest="$1" name="$2"
  mkdir -p "$(dirname "${dest}")"
  printf 'OrionHEN-STUB:%s\0' "${name}" > "${dest}"
  warn "STUB ${dest} (not for real hardware)"
}

need_sdk() {
  [[ -n "${PS5_PAYLOAD_SDK}" && -d "${PS5_PAYLOAD_SDK}" ]] || \
    die "PS5_PAYLOAD_SDK required to build from source"
  export PS5_PAYLOAD_SDK
  export PATH="${PS5_PAYLOAD_SDK}/bin:${PATH}"
}

init_submodules() {
  log "git submodule update --init --recursive"
  git -C "${ROOT}" submodule update --init --recursive
  ok "submodules ready"
}

sync_kstuff() {
  local dest="${SOURCE}/bootstrapper/assets/kstuff.elf"

  if [[ "${FROM_SOURCE}" -eq 0 ]]; then
    log "kstuff: download kstuff-lite release"
    if download "${KSTUFF_URL}" "${dest}"; then
      ok "kstuff.elf (kstuff-lite v1.09)"
      return 0
    fi
    warn "download failed, trying submodule build"
  fi

  if [[ -d "${TP}/kstuff-lite" ]]; then
    need_sdk
    log "kstuff: build third_party/kstuff-lite (best-effort)"
    if [[ -x "${TP}/kstuff-lite/ci-ps5-kstuff-ldr.sh" ]]; then
      (cd "${TP}/kstuff-lite" && bash ./ci-ps5-kstuff-ldr.sh) || true
    elif [[ -f "${TP}/kstuff-lite/Makefile" ]]; then
      make -C "${TP}/kstuff-lite" -j4 || true
    fi
    local found
    found="$(find "${TP}/kstuff-lite" -name 'kstuff.elf' 2>/dev/null | head -1 || true)"
    if [[ -n "${found}" ]]; then
      place "${found}" "${dest}"
      return 0
    fi
  fi

  if [[ "${STUB_MISSING}" -eq 1 ]]; then
    stub "${dest}" "kstuff.elf"
    return 0
  fi
  die "kstuff.elf unavailable (prefer release download)"
}

main() {
  log "OrionHEN vendor sync"
  echo "  third_party = ${TP}"

  mkdir -p \
    "${SOURCE}/daemon/assets" \
    "${SOURCE}/bootstrapper/assets" \
    "${VENDOR}"

  if [[ "${INIT_SUBMODULES}" -eq 1 ]]; then
    init_submodules
  elif [[ ! -d "${TP}/kstuff-lite" ]]; then
    warn "submodules not checked out — run: git submodule update --init --recursive"
    warn "release downloads still work without submodules"
  fi

  # Note: elfldr.elf is no longer vendored. third_party/elfldr is kept only as
  # an optional source reference for the external 9021 loader.
  sync_kstuff

  if [[ -f "${SOURCE}/bootstrapper/assets/kstuff.elf" ]]; then
    cp -f "${SOURCE}/bootstrapper/assets/kstuff.elf" "${VENDOR}/kstuff.elf"
  fi

  # Drop leftover 9021 blob if present from older trees
  rm -f "${SOURCE}/util/assets/elfldr.elf" "${VENDOR}/elfldr.elf"

  log "Vendor sync done"
  ls -lah \
    "${SOURCE}/bootstrapper/assets/kstuff.elf" \
    2>/dev/null || true
}

main
