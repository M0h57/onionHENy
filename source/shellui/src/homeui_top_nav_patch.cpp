/* Copyright (C) 2025 OnionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include "homeui_top_nav_patch.hpp"

#include "defs.h"
#include "detour.h"
#include "hooked_funcs.hpp"
#include "ipc.hpp"
#include <elf/nid/sha1.hpp>

#include <atomic>
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <string.h>

namespace {

constexpr unsigned char kHermesMagic[] = {0xc6, 0x1f, 0xbc, 0x03,
                                          0xc1, 0x03, 0x19, 0x1f};
constexpr unsigned char kRnpsMagic[] = {'R', 'N', 'P', 'S',
                                        'H', 'E', 'D', 'R'};

constexpr size_t kRnpsPayloadOffsetField = 0x1c;
constexpr size_t kRnpsFallbackPayloadOffset = 0xb20;
constexpr size_t kHbcFileLengthOffset = 0x20;
constexpr size_t kHbcFooterSha1Size = 20;
constexpr size_t kSupportedHomeUiHbcFileLength = 0x1b2cc8;
constexpr const char *kOnionHenTopNavIconPath =
    "/system_ex/vsh_asset/onionhen.png";

std::atomic<bool> g_homeui_top_nav_reload_pending{false};
std::atomic<bool> g_logged_non_homeui_skip{false};
void (*g_react_button_set_icon_source_orig)(MonoObject *instance,
                                            MonoObject *source) = nullptr;
void (*g_react_button_set_inverted_icon_source)(MonoObject *instance,
                                                MonoObject *source) = nullptr;

struct HbcView {
  unsigned char *data;
  size_t size;
  size_t base_offset;
};

struct BytePatch {
  const char *name;
  size_t offset;
  const unsigned char *expected;
  const unsigned char *replacement;
  size_t size;
};

static bool range_contains(size_t size, size_t offset, size_t len) {
  return offset <= size && len <= size - offset;
}

static bool bytes_equal(const unsigned char *lhs, const unsigned char *rhs,
                        size_t len) {
  return memcmp(lhs, rhs, len) == 0;
}

static uint32_t read_u32le(const unsigned char *p) {
  return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool has_magic(const unsigned char *data, size_t size,
                      const unsigned char *magic, size_t magic_size) {
  return range_contains(size, 0, magic_size) &&
         bytes_equal(data, magic, magic_size);
}

static bool hbc_at(unsigned char *buffer, size_t available, size_t offset,
                   HbcView *out) {
  if (!range_contains(available, offset, sizeof(kHermesMagic)) ||
      !has_magic(buffer + offset, available - offset, kHermesMagic,
                 sizeof(kHermesMagic))) {
    return false;
  }

  out->data = buffer + offset;
  out->size = available - offset;
  out->base_offset = offset;
  return true;
}

static bool locate_hbc(unsigned char *buffer, size_t visible_size,
                       size_t capacity, HbcView *out) {
  const size_t available = capacity > visible_size ? capacity : visible_size;

  if (hbc_at(buffer, available, 0, out)) {
    return true;
  }

  if (!has_magic(buffer, available, kRnpsMagic, sizeof(kRnpsMagic))) {
    return false;
  }

  size_t rnps_payload_offset = kRnpsFallbackPayloadOffset;
  if (range_contains(available, kRnpsPayloadOffsetField, sizeof(uint32_t))) {
    const uint32_t declared_offset =
        read_u32le(buffer + kRnpsPayloadOffsetField);
    if (declared_offset > 0 && declared_offset < available) {
      rnps_payload_offset = declared_offset;
    }
  }

  if (hbc_at(buffer, available, rnps_payload_offset, out)) {
    return true;
  }

  if (rnps_payload_offset != kRnpsFallbackPayloadOffset &&
      hbc_at(buffer, available, kRnpsFallbackPayloadOffset, out)) {
    return true;
  }

  return false;
}

static bool read_hbc_file_length(const HbcView &hbc, size_t *file_length) {
  if (!range_contains(hbc.size, kHbcFileLengthOffset, sizeof(uint32_t))) {
    return false;
  }

  const uint32_t declared_file_length =
      read_u32le(hbc.data + kHbcFileLengthOffset);
  if (declared_file_length <= kHbcFooterSha1Size ||
      declared_file_length > hbc.size) {
    return false;
  }

  *file_length = declared_file_length;
  return true;
}

static bool hbc_bytes_at(const HbcView &hbc, size_t offset,
                         const char *expected) {
  const size_t len = strlen(expected);
  return range_contains(hbc.size, offset, len) &&
         bytes_equal(hbc.data + offset,
                     reinterpret_cast<const unsigned char *>(expected), len);
}

static bool is_supported_homeui_hbc(const HbcView &hbc, size_t file_length) {
  return file_length == kSupportedHomeUiHbcFileLength &&
         hbc_bytes_at(hbc, 0x450f2, "NPXS40002") &&
         hbc_bytes_at(hbc, 0x443fb, "ApplicationErrorEventTrigger") &&
         hbc_bytes_at(hbc, 0x53b3e, "pshomeui:navigateToHome");
}

static bool validate_patch(const HbcView &hbc, const BytePatch &patch,
                           bool *already_applied) {
  if (!range_contains(hbc.size, patch.offset, patch.size)) {
#if SHELL_DEBUG == 1
    shellui_log("homeui_top_nav_patch: %s out of range at hbc+0x%llx",
                patch.name, (unsigned long long)patch.offset);
#endif
    return false;
  }

  const unsigned char *current = hbc.data + patch.offset;
  if (bytes_equal(current, patch.replacement, patch.size)) {
    *already_applied = true;
    return true;
  }

  if (bytes_equal(current, patch.expected, patch.size)) {
    *already_applied = false;
    return true;
  }

#if SHELL_DEBUG == 1
  shellui_log("homeui_top_nav_patch: %s mismatch at hbc+0x%llx; skip",
              patch.name, (unsigned long long)patch.offset);
#endif
  return false;
}

static void apply_patch(const HbcView &hbc, const BytePatch &patch) {
  if (bytes_equal(hbc.data + patch.offset, patch.expected, patch.size)) {
    memcpy(hbc.data + patch.offset, patch.replacement, patch.size);
  }
}

static void update_hbc_footer_sha1(HbcView &hbc, size_t file_length) {
  const size_t footer_offset = file_length - kHbcFooterSha1Size;
  SHA1_CTX ctx;

  SHA1Init(&ctx);
  SHA1Update(&ctx, hbc.data, (uint32_t)footer_offset);
  SHA1Final(hbc.data + footer_offset, &ctx);
}

static bool is_homeui_top_nav_icon_source(MonoObject *source) {
#if SHELLUI_HOMEUI_TOP_NAV_PATCH == 1
  if (!source || !mono_object_to_string) {
    return false;
  }

  MonoObject *exception = nullptr;
  MonoString *text = mono_object_to_string(source, &exception);
  if (exception || !text) {
#if SHELL_DEBUG == 1
    shellui_log("homeui_top_nav_patch: ReactButton icon source ToString failed");
#endif
    return false;
  }

  return Mono_to_String(text).find(kOnionHenTopNavIconPath) !=
         std::string::npos;
#else
  (void)source;
  return false;
#endif
}

static void ReactButtonShadowNode_SetIconSource_Hook(MonoObject *instance,
                                                     MonoObject *source) {
  if (g_react_button_set_icon_source_orig) {
    g_react_button_set_icon_source_orig(instance, source);
  }

  if (!shellui_hooks_are_ready() || !instance || !source ||
      !g_react_button_set_inverted_icon_source ||
      !is_homeui_top_nav_icon_source(source)) {
    return;
  }

  g_react_button_set_inverted_icon_source(instance, source);
#if SHELL_DEBUG == 1
  shellui_log("homeui_top_nav_patch: mirrored top-nav icon to invertedIcon");
#endif
}

} // namespace

void install_homeui_top_nav_hooks(MonoImage *react_pui) {
#if SHELLUI_HOMEUI_TOP_NAV_PATCH == 1
  if (!react_pui) {
    shellui_log("homeui_top_nav_patch: ReactNative.PUI image missing");
    return;
  }

  g_react_button_set_inverted_icon_source =
      reinterpret_cast<void (*)(MonoObject *, MonoObject *)>(
          Get_Address_of_Method(react_pui, "ReactNative.Views.UI3.View",
                                "ReactButtonShadowNode",
                                "SetinvertedIconSource", 1));
#if SHELL_DEBUG == 1
  shellui_log(g_react_button_set_inverted_icon_source
                  ? "homeui_top_nav_patch: SetinvertedIconSource found"
                  : "homeui_top_nav_patch: SetinvertedIconSource missing");
#endif

  const uint64_t set_icon_source =
      Get_Address_of_Method(react_pui, "ReactNative.Views.UI3.View",
                            "ReactButtonShadowNode", "SetIconSource", 1);
  if (!set_icon_source) {
    shellui_log("homeui_top_nav_patch: SetIconSource missing");
    return;
  }

  const bool installed = InstallDetour(
      set_icon_source,
      reinterpret_cast<void *>(&ReactButtonShadowNode_SetIconSource_Hook),
      reinterpret_cast<void **>(&g_react_button_set_icon_source_orig));
  shellui_log(installed ? "homeui_top_nav_patch: SetIconSource hooked"
                        : "homeui_top_nav_patch: SetIconSource detour failed");
#else
  (void)react_pui;
#endif
}

void patch_homeui_top_nav(unsigned char *buffer, int *size_ptr,
                          int buffer_capacity) {
#if SHELLUI_HOMEUI_TOP_NAV_PATCH == 1
  if (!buffer || !size_ptr || *size_ptr <= 0) {
    return;
  }

  HbcView hbc = {};
  const size_t visible_size = (size_t)*size_ptr;
  const size_t capacity =
      buffer_capacity > *size_ptr ? (size_t)buffer_capacity : visible_size;
  if (!locate_hbc(buffer, visible_size, capacity, &hbc)) {
#if SHELL_DEBUG == 1
    const unsigned char b0 = visible_size > 0 ? buffer[0] : 0;
    const unsigned char b1 = visible_size > 1 ? buffer[1] : 0;
    const unsigned char b2 = visible_size > 2 ? buffer[2] : 0;
    const unsigned char b3 = visible_size > 3 ? buffer[3] : 0;
    shellui_log("homeui_top_nav_patch: no HBC candidate (size=%d capacity=%d "
                "head=%02x %02x %02x %02x)",
                *size_ptr, buffer_capacity, b0, b1, b2, b3);
#endif
    return;
  }

  size_t hbc_file_length = 0;
  if (!read_hbc_file_length(hbc, &hbc_file_length)) {
#if SHELL_DEBUG == 1
    shellui_log("homeui_top_nav_patch: invalid HBC length at base+0x%llx",
                (unsigned long long)hbc.base_offset);
#endif
    return;
  }

  if (!is_supported_homeui_hbc(hbc, hbc_file_length)) {
#if SHELL_DEBUG == 1
    if (!g_logged_non_homeui_skip.exchange(true, std::memory_order_relaxed)) {
      shellui_log("homeui_top_nav_patch: skip non-NPXS40002 HBC "
                  "(hbc_base=0x%llx hbc_file_length=0x%llx)",
                  (unsigned long long)hbc.base_offset,
                  (unsigned long long)hbc_file_length);
    }
#endif
    return;
  }

#if SHELL_DEBUG == 1
  shellui_log("homeui_top_nav_patch: HBC candidate hbc_base=0x%llx "
              "hbc_file_length=0x%llx size=%d capacity=%d",
              (unsigned long long)hbc.base_offset,
              (unsigned long long)hbc_file_length, *size_ptr, buffer_capacity);
#endif

  static const unsigned char kOldIconOrder[] = {
      0x54, 0xfa, 0x1a, 0x5e, 0x1b, 0x0d, 0x15, 0x1b, 0x16};
  static const unsigned char kNewIconOrder[] = {
      0x54, 0x5e, 0x1b, 0xfa, 0x1a, 0x0d, 0x15, 0x1b, 0x16};

  static const unsigned char kOriginalFpsFactory[] = {0x62, 0x01, 0x01,
                                                      0xaf, 0x1d};
  static const unsigned char kOnionHenIconFactory[] = {
      0x62, 0x01, 0x01, 0xad, 0x1d};

  /*
   * The visible Settings slot also uses Function #7590. Patching #7590's
   * iconId would turn both Settings and the inserted slot into OnionHEN. Route
   * the reused Fps slot to the hidden ApplicationErrorEventTrigger icon-button,
   * then repurpose that hidden component as the OnionHEN top-nav button:
   *
   *   object icon value: download_error -> /system_ex/vsh_asset/onionhen.png
   *   object title:      Trigger AppError -> spaces
   *
   * Its original AppError callback is replaced with HomeUI's existing
   * useInteractivePress hook. The link uses a same-length private URI
   * (the old download_error string content -> OnionHEN?NavUI), which
   * hook_boot.cpp routes to the Debug Settings legacy host.
   *
   * ReactButtonShadowNode_SetIconSource_Hook mirrors this same ImageSource into
   * invertedIcon so the focused state has an image too.
   */
  static const unsigned char kOldCustomIconValue[] = {0xba, 0x01};
  static const unsigned char kNewCustomIconValue[] = {0x47, 0x0f};
  static const unsigned char kOldTopNavPressUri[] = {
      'd', 'o', 'w', 'n', 'l', 'o', 'a', 'd', '_', 'e', 'r', 'r', 'o', 'r'};
  static const unsigned char kNewTopNavPressUri[] = {
      'O', 'n', 'i', 'o', 'n', 'H', 'E', 'N', '?', 'N', 'a', 'v', 'U', 'I'};
  static const unsigned char kOldCustomIconUri[] = {
      'h', 'o', 'm', 'e', 'u', 'i', ' ', 'A', 'p', 'p', 'l',
      'i', 'c', 'a', 't', 'i', 'o', 'n', 'E', 'r', 'r', 'o',
      'r', 'E', 'v', 'e', 'n', 't', ' ', 't', 'e', 's', 't'};
  static const unsigned char kNewCustomIconUri[] = {
      '/', 's', 'y', 's', 't', 'e', 'm', '_', 'e', 'x', '/',
      'v', 's', 'h', '_', 'a', 's', 's', 'e', 't', '/', 'o',
      'n', 'i', 'o', 'n', 'h', 'e', 'n', '.', 'p', 'n', 'g'};
  static const unsigned char kOldCustomTitle[] = {
      'T', 'r', 'i', 'g', 'g', 'e', 'r', ' ',
      'A', 'p', 'p', 'E', 'r', 'r', 'o', 'r'};
  static const unsigned char kNewCustomTitle[] = {
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  static const unsigned char kOldAppErrorTriggerBody[] = {
      0x32, 0x04, 0x29, 0x00, 0x00, 0x2e, 0x01, 0x00, 0x06, 0x34, 0x01,
      0x01, 0x01, 0x6e, 0x74, 0x03, 0x4f, 0x01, 0x01, 0x03, 0x35, 0x01,
      0x01, 0x02, 0xdb, 0x15, 0x2a, 0x04, 0x00, 0x01, 0x2e, 0x01, 0x00,
      0x09, 0x34, 0x02, 0x01, 0x03, 0x7f, 0x2e, 0x00, 0x00, 0x08, 0x34,
      0x01, 0x00, 0x01, 0x6e, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00, 0x7e,
      0x10, 0x70, 0x19, 0x62, 0x04, 0x04, 0xae, 0x1d, 0x39, 0x00, 0x04,
      0x01, 0xb6, 0x00, 0x52, 0x00, 0x02, 0x03, 0x01, 0x00, 0x5a, 0x00};
  static const unsigned char kOnionHenTriggerBody[] = {
      0x29, 0x00, 0x00, 0x2e, 0x01, 0x00, 0x05, 0x35, 0x04, 0x01, 0x01,
      0x5a, 0x2a, 0x03, 0x01, 0x71, 0x02, 0xba, 0x01, 0x3e, 0x01, 0x02,
      0xa5, 0x17, 0x74, 0x03, 0x51, 0x05, 0x04, 0x03, 0x01, 0x2e, 0x01,
      0x00, 0x09, 0x34, 0x02, 0x01, 0x02, 0x7f, 0x2e, 0x01, 0x00, 0x08,
      0x34, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00,
      0x7e, 0x10, 0x70, 0x19, 0x39, 0x00, 0x05, 0x01, 0xb6, 0x00, 0x52,
      0x00, 0x02, 0x03, 0x01, 0x00, 0x5a, 0x00, 0x74, 0x00, 0x74, 0x00};
  static_assert(sizeof(kOldAppErrorTriggerBody) ==
                    sizeof(kOnionHenTriggerBody),
                "HomeUI top-nav trigger body patch must be equal length");

  static const BytePatch kPatches[] = {
      {"home icon order", 0xbcf40, kOldIconOrder, kNewIconOrder,
       sizeof(kOldIconOrder)},
      {"top-nav icon factory", 0x175cbd, kOriginalFpsFactory,
       kOnionHenIconFactory, sizeof(kOriginalFpsFactory)},
      {"top-nav press uri", 0x3a01f, kOldTopNavPressUri, kNewTopNavPressUri,
       sizeof(kOldTopNavPressUri)},
      {"custom icon value", 0xc0451, kOldCustomIconValue, kNewCustomIconValue,
       sizeof(kOldCustomIconValue)},
      {"custom icon uri", 0x533ed, kOldCustomIconUri, kNewCustomIconUri,
       sizeof(kOldCustomIconUri)},
      {"custom icon title", 0x49cd3, kOldCustomTitle, kNewCustomTitle,
       sizeof(kOldCustomTitle)},
      {"top-nav trigger body", 0x17601a, kOldAppErrorTriggerBody,
       kOnionHenTriggerBody, sizeof(kOldAppErrorTriggerBody)},
  };

  bool any_change = false;
  for (size_t i = 0; i < sizeof(kPatches) / sizeof(kPatches[0]); ++i) {
    bool already_applied = false;
    if (!validate_patch(hbc, kPatches[i], &already_applied)) {
      return;
    }
    any_change = any_change || !already_applied;
  }

  if (!any_change) {
#if SHELL_DEBUG == 1
    shellui_log("homeui_top_nav_patch: already applied (hbc_base=0x%llx)",
                (unsigned long long)hbc.base_offset);
#endif
    return;
  }

  for (size_t i = 0; i < sizeof(kPatches) / sizeof(kPatches[0]); ++i) {
    apply_patch(hbc, kPatches[i]);
  }
  update_hbc_footer_sha1(hbc, hbc_file_length);

#if SHELL_DEBUG == 1
  shellui_log("homeui_top_nav_patch: activated OnionHEN top-nav slot "
              "(hbc_base=0x%llx)",
              (unsigned long long)hbc.base_offset);
#endif
#else
  (void)buffer;
  (void)size_ptr;
  (void)buffer_capacity;
#endif
}

void shellui_request_homeui_top_nav_reload(void) {
#if SHELLUI_HOMEUI_TOP_NAV_PATCH == 1
  g_homeui_top_nav_reload_pending.store(true, std::memory_order_release);
  shellui_log("homeui_top_nav_patch: queued NPXS40002 reload for next UI tick");
#endif
}

void shellui_poll_homeui_top_nav_reload(void) {
#if SHELLUI_HOMEUI_TOP_NAV_PATCH == 1
  if (!g_homeui_top_nav_reload_pending.exchange(false,
                                                std::memory_order_acq_rel)) {
    return;
  }

  shellui_log("homeui_top_nav_patch: applying NPXS40002 reload on UI thread");
  ReloadRNPSApp("NPXS40002");
#endif
}
