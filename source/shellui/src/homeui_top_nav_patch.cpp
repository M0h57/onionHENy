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
constexpr size_t kHbcVersionOffset = 0x08;
constexpr size_t kHbcSourceHashOffset = 0x0c;
constexpr size_t kHbcSourceHashSize = 20;
constexpr size_t kHbcFileLengthOffset = 0x20;
constexpr size_t kHbcFooterSha1Size = 20;
constexpr const char *kOnionHenTopNavIconPath =
    "/system_ex/vsh_asset/onionhen.png";

std::atomic<bool> g_homeui_top_nav_reload_pending{false};
std::atomic<bool> g_logged_non_homeui_skip{false};
std::atomic<bool> g_logged_unsupported_homeui_skip{false};
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
  const unsigned char *alternate_expected;
  const unsigned char *second_alternate_expected;
  const unsigned char *replacement;
  size_t size;
};

struct HomeUiPatchOffsets {
  size_t title_id;
  size_t app_error_event_trigger;
  size_t navigate_to_home;
  size_t home_icon_order;
  size_t fps_factory;
  size_t download_error_string;
  size_t custom_icon_value;
  size_t custom_icon_uri;
  size_t top_nav_link_uri;
  size_t custom_title_value;
  size_t fps_body;
};

struct HomeUiPatchProfile {
  const char *name;
  uint32_t hbc_version;
  size_t file_length;
  unsigned char source_hash[kHbcSourceHashSize];
  HomeUiPatchOffsets offsets;
};

static const HomeUiPatchProfile kHomeUiPatchProfiles[] = {
    {
        "11.6 NPXS40002 HomeUI",
        89,
        0x1b2cc8,
        {0xf3, 0x21, 0xf8, 0x3f, 0x91, 0x43, 0x03,
         0x5f, 0x5d, 0x97, 0xee, 0x5a, 0xd9, 0x8c,
         0xeb, 0x75, 0x13, 0x3c, 0x89, 0x0e},
        {
            0x450f2,
            0x443fb,
            0x53b3e,
            0xbcf40,
            0x175cbd,
            0x3a01f,
            0xc0451,
            0x533ed,
            0x49cd3,
            0xc0455,
            0x1760b3,
        },
    },
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

static bool read_hbc_version(const HbcView &hbc, uint32_t *version) {
  if (!range_contains(hbc.size, kHbcVersionOffset, sizeof(uint32_t))) {
    return false;
  }

  *version = read_u32le(hbc.data + kHbcVersionOffset);
  return true;
}

static bool hbc_bytes_at(const HbcView &hbc, size_t offset,
                         const char *expected) {
  const size_t len = strlen(expected);
  return range_contains(hbc.size, offset, len) &&
         bytes_equal(hbc.data + offset,
                     reinterpret_cast<const unsigned char *>(expected), len);
}

static bool hbc_source_hash_matches(const HbcView &hbc,
                                    const unsigned char *source_hash) {
  return range_contains(hbc.size, kHbcSourceHashOffset, kHbcSourceHashSize) &&
         bytes_equal(hbc.data + kHbcSourceHashOffset, source_hash,
                     kHbcSourceHashSize);
}

static void format_hbc_source_hash(const HbcView &hbc, char *out,
                                   size_t out_size) {
  static const char kHex[] = "0123456789abcdef";
  if (!out || out_size == 0) {
    return;
  }

  out[0] = '\0';
  if (out_size < kHbcSourceHashSize * 2 + 1 ||
      !range_contains(hbc.size, kHbcSourceHashOffset, kHbcSourceHashSize)) {
    return;
  }

  const unsigned char *hash = hbc.data + kHbcSourceHashOffset;
  for (size_t i = 0; i < kHbcSourceHashSize; ++i) {
    out[i * 2] = kHex[hash[i] >> 4];
    out[i * 2 + 1] = kHex[hash[i] & 0x0f];
  }
  out[kHbcSourceHashSize * 2] = '\0';
}

static bool validate_homeui_profile_markers(const HbcView &hbc,
                                            const HomeUiPatchProfile &profile) {
  return hbc_bytes_at(hbc, profile.offsets.title_id, "NPXS40002") &&
         hbc_bytes_at(hbc, profile.offsets.app_error_event_trigger,
                      "ApplicationErrorEventTrigger") &&
         hbc_bytes_at(hbc, profile.offsets.navigate_to_home,
                      "pshomeui:navigateToHome");
}

static const HomeUiPatchProfile *
find_homeui_patch_profile(const HbcView &hbc, size_t file_length,
                          uint32_t hbc_version) {
  for (size_t i = 0;
       i < sizeof(kHomeUiPatchProfiles) / sizeof(kHomeUiPatchProfiles[0]);
       ++i) {
    const HomeUiPatchProfile &profile = kHomeUiPatchProfiles[i];
    if (file_length == profile.file_length &&
        hbc_version == profile.hbc_version &&
        hbc_source_hash_matches(hbc, profile.source_hash) &&
        validate_homeui_profile_markers(hbc, profile)) {
      return &profile;
    }
  }

  return nullptr;
}

static bool looks_like_profiled_homeui(const HbcView &hbc) {
  for (size_t i = 0;
       i < sizeof(kHomeUiPatchProfiles) / sizeof(kHomeUiPatchProfiles[0]);
       ++i) {
    const HomeUiPatchProfile &profile = kHomeUiPatchProfiles[i];
    if (validate_homeui_profile_markers(hbc, profile)) {
      return true;
    }
  }

  return false;
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

  if (bytes_equal(current, patch.expected, patch.size) ||
      (patch.alternate_expected &&
       bytes_equal(current, patch.alternate_expected, patch.size)) ||
      (patch.second_alternate_expected &&
       bytes_equal(current, patch.second_alternate_expected, patch.size))) {
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
  const unsigned char *current = hbc.data + patch.offset;
  if (!bytes_equal(current, patch.replacement, patch.size)) {
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

  uint32_t hbc_version = 0;
  if (!read_hbc_version(hbc, &hbc_version)) {
#if SHELL_DEBUG == 1
    shellui_log("homeui_top_nav_patch: invalid HBC version at base+0x%llx",
                (unsigned long long)hbc.base_offset);
#endif
    return;
  }

  const HomeUiPatchProfile *profile =
      find_homeui_patch_profile(hbc, hbc_file_length, hbc_version);
  if (!profile) {
#if SHELL_DEBUG == 1
    if (looks_like_profiled_homeui(hbc)) {
      if (!g_logged_unsupported_homeui_skip.exchange(
              true, std::memory_order_relaxed)) {
        char source_hash[kHbcSourceHashSize * 2 + 1];
        format_hbc_source_hash(hbc, source_hash, sizeof(source_hash));
        shellui_log("homeui_top_nav_patch: unsupported HomeUI HBC "
                    "(hbc_base=0x%llx version=%u file_length=0x%llx "
                    "source_hash=%s)",
                    (unsigned long long)hbc.base_offset, hbc_version,
                    (unsigned long long)hbc_file_length, source_hash);
      }
    } else if (!g_logged_non_homeui_skip.exchange(true,
                                                   std::memory_order_relaxed)) {
      shellui_log("homeui_top_nav_patch: skip non-HomeUI HBC "
                  "(hbc_base=0x%llx version=%u file_length=0x%llx)",
                  (unsigned long long)hbc.base_offset, hbc_version,
                  (unsigned long long)hbc_file_length);
    }
#endif
    return;
  }

#if SHELL_DEBUG == 1
  shellui_log("homeui_top_nav_patch: matched profile '%s' hbc_base=0x%llx "
              "version=%u hbc_file_length=0x%llx size=%d capacity=%d",
              profile->name,
              (unsigned long long)hbc.base_offset,
              hbc_version, (unsigned long long)hbc_file_length, *size_ptr,
              buffer_capacity);
#endif

  static const unsigned char kOldIconOrder[] = {
      0x54, 0xfa, 0x1a, 0x5e, 0x1b, 0x0d, 0x15, 0x1b, 0x16};
  static const unsigned char kLegacyAliasedIconOrder[] = {
      0x54, 0x5e, 0x1b, 0xfa, 0x1a, 0x0d, 0x15, 0x1b, 0x16};
  static const unsigned char kLegacyHiddenIconOrder[] = {
      0x54, 0x5e, 0x1b, 0xdc, 0x15, 0x0d, 0x15, 0x1b, 0x16};
  static const unsigned char kNewIconOrder[] = {
      0x54, 0x5e, 0x1b, 0xfa, 0x1a, 0x0d, 0x15, 0x1b, 0x16};

  static const unsigned char kOriginalFpsFactory[] = {0x62, 0x01, 0x01,
                                                      0xaf, 0x1d};
  static const unsigned char kLegacyAliasedFpsFactory[] = {
      0x62, 0x01, 0x01, 0xad, 0x1d};

  /*
   * The visible Settings slot uses Function #7590, so patching its iconId would
   * turn both Settings and the inserted slot into OnionHEN. Instead, keep the
   * normal top-nav component names and make the dormant Fps slot render the
   * OnionHEN button:
   *
   *   object icon value: download_error -> /system_ex/vsh_asset/onionhen.png
   *   object title:      Trigger AppError -> empty string
   *
   * The Fps body is replaced with a short icon-button body that uses HomeUI's
   * existing useInteractivePress hook. Keep the stock download_error icon id
   * string intact; the link uses the private Trigger AppError string slot as a
   * padded OnionHEN?NavUI URI, which hook_boot.cpp routes to the Debug Settings
   * legacy host.
   *
   * ReactButtonShadowNode_SetIconSource_Hook mirrors this same ImageSource into
   * invertedIcon so the focused state has an image too.
   *
   * Older test builds aliased Fps to ApplicationErrorEventTrigger or inserted
   * ApplicationErrorEventTrigger directly. Accept and repair those in-memory
   * shapes, but keep Fps on its original export so profile modal rendering sees
   * the expected HomeUI module shape.
   */
  static const unsigned char kOldCustomIconValue[] = {0xba, 0x01};
  static const unsigned char kNewCustomIconValue[] = {0x47, 0x0f};
  static const unsigned char kLegacyTopNavPressUri[] = {
      'O', 'n', 'i', 'o', 'n', 'H', 'E', 'N', '?', 'N', 'a', 'v', 'U', 'I'};
  static const unsigned char kStockDownloadErrorString[] = {
      'd', 'o', 'w', 'n', 'l', 'o', 'a', 'd', '_', 'e', 'r', 'r', 'o', 'r'};
  static const unsigned char kOldCustomIconUri[] = {
      'h', 'o', 'm', 'e', 'u', 'i', ' ', 'A', 'p', 'p', 'l',
      'i', 'c', 'a', 't', 'i', 'o', 'n', 'E', 'r', 'r', 'o',
      'r', 'E', 'v', 'e', 'n', 't', ' ', 't', 'e', 's', 't'};
  static const unsigned char kNewCustomIconUri[] = {
      '/', 's', 'y', 's', 't', 'e', 'm', '_', 'e', 'x', '/',
      'v', 's', 'h', '_', 'a', 's', 's', 'e', 't', '/', 'o',
      'n', 'i', 'o', 'n', 'h', 'e', 'n', '.', 'p', 'n', 'g'};
  static const unsigned char kOldTopNavLinkUri[] = {
      'T', 'r', 'i', 'g', 'g', 'e', 'r', ' ',
      'A', 'p', 'p', 'E', 'r', 'r', 'o', 'r'};
  static const unsigned char kLegacyBlankTopNavLinkUri[] = {
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
      ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
  static const unsigned char kNewTopNavLinkUri[] = {
      'O', 'n', 'i', 'o', 'n', 'H', 'E', 'N',
      '?', 'N', 'a', 'v', 'U', 'I', ' ', ' '};
  static const unsigned char kOldCustomTitleValue[] = {0x17, 0x0b};
  static const unsigned char kNewCustomTitleValue[] = {0xff, 0x00};
  static const unsigned char kLegacyOnionHenButtonBody[] = {
      0x29, 0x00, 0x00, 0x2e, 0x01, 0x00, 0x05, 0x35, 0x04, 0x01, 0x01,
      0x5a, 0x2a, 0x03, 0x01, 0x71, 0x02, 0xba, 0x01, 0x3e, 0x01, 0x02,
      0xa5, 0x17, 0x74, 0x03, 0x51, 0x05, 0x04, 0x03, 0x01, 0x2e, 0x01,
      0x00, 0x09, 0x34, 0x02, 0x01, 0x02, 0x7f, 0x2e, 0x01, 0x00, 0x08,
      0x34, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00,
      0x7e, 0x10, 0x70, 0x19, 0x39, 0x00, 0x05, 0x01, 0xb6, 0x00, 0x52,
      0x00, 0x02, 0x03, 0x01, 0x00, 0x5a, 0x00, 0x74, 0x00, 0x74, 0x00};
  static const unsigned char kOnionHenButtonBody[] = {
      0x29, 0x00, 0x00, 0x2e, 0x01, 0x00, 0x05, 0x35, 0x04, 0x01, 0x01,
      0x5a, 0x2a, 0x03, 0x01, 0x71, 0x02, 0x17, 0x0b, 0x3e, 0x01, 0x02,
      0xa5, 0x17, 0x74, 0x03, 0x51, 0x05, 0x04, 0x03, 0x01, 0x2e, 0x01,
      0x00, 0x09, 0x34, 0x02, 0x01, 0x02, 0x7f, 0x2e, 0x01, 0x00, 0x08,
      0x34, 0x01, 0x01, 0x01, 0x6e, 0x01, 0x00, 0x03, 0x00, 0x03, 0x00,
      0x7e, 0x10, 0x70, 0x19, 0x39, 0x00, 0x05, 0x01, 0xb6, 0x00, 0x52,
      0x00, 0x02, 0x03, 0x01, 0x00, 0x5a, 0x00, 0x74, 0x00, 0x74, 0x00};
  static const unsigned char kOldFpsBodyPrefix[] = {
      0x29, 0x00, 0x00, 0x2e, 0x01, 0x00, 0x02, 0x34, 0x04, 0x01, 0x01,
      0xe8, 0x2e, 0x02, 0x00, 0x00, 0x35, 0x02, 0x02, 0x02, 0xb7, 0x1d,
      0x74, 0x03, 0x51, 0x02, 0x04, 0x03, 0x02, 0x35, 0x05, 0x02, 0x03,
      0x4b, 0x1c, 0x34, 0x04, 0x01, 0x04, 0xf5, 0x32, 0x01, 0x62, 0x02,
      0x01, 0xb0, 0x1d, 0x07, 0x01, 0x00, 0x00, 0x52, 0x04, 0x04, 0x03,
      0x02, 0x01, 0x35, 0x02, 0x05, 0x05, 0x17, 0x2c, 0x71, 0x01, 0x25,
      0x0c, 0x51, 0x01, 0x02, 0x05, 0x01, 0x8e, 0x07, 0x01, 0x75, 0x01};
  static_assert(sizeof(kOldFpsBodyPrefix) == sizeof(kOnionHenButtonBody),
                "HomeUI top-nav Fps body patch must fit prefix length");

  const BytePatch kPatches[] = {
      {"home icon order", profile->offsets.home_icon_order, kOldIconOrder,
       kLegacyAliasedIconOrder, kLegacyHiddenIconOrder, kNewIconOrder,
       sizeof(kOldIconOrder)},
      {"legacy Fps factory repair", profile->offsets.fps_factory,
       kLegacyAliasedFpsFactory, nullptr, nullptr, kOriginalFpsFactory,
       sizeof(kOriginalFpsFactory)},
      {"download_error string repair", profile->offsets.download_error_string,
       kLegacyTopNavPressUri, nullptr, nullptr, kStockDownloadErrorString,
       sizeof(kStockDownloadErrorString)},
      {"custom icon value", profile->offsets.custom_icon_value,
       kOldCustomIconValue, nullptr, nullptr, kNewCustomIconValue,
       sizeof(kOldCustomIconValue)},
      {"custom icon uri", profile->offsets.custom_icon_uri, kOldCustomIconUri,
       nullptr, nullptr, kNewCustomIconUri, sizeof(kOldCustomIconUri)},
      {"top-nav link uri", profile->offsets.top_nav_link_uri,
       kOldTopNavLinkUri, kLegacyBlankTopNavLinkUri, nullptr,
       kNewTopNavLinkUri, sizeof(kOldTopNavLinkUri)},
      {"custom title value", profile->offsets.custom_title_value,
       kOldCustomTitleValue, nullptr, nullptr, kNewCustomTitleValue,
       sizeof(kOldCustomTitleValue)},
      {"top-nav Fps body", profile->offsets.fps_body, kOldFpsBodyPrefix,
       kLegacyOnionHenButtonBody, nullptr,
       kOnionHenButtonBody, sizeof(kOldFpsBodyPrefix)},
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
    shellui_log("homeui_top_nav_patch: already applied profile='%s' "
                "(hbc_base=0x%llx)",
                profile->name, (unsigned long long)hbc.base_offset);
#endif
    return;
  }

  for (size_t i = 0; i < sizeof(kPatches) / sizeof(kPatches[0]); ++i) {
    apply_patch(hbc, kPatches[i]);
  }
  update_hbc_footer_sha1(hbc, hbc_file_length);

#if SHELL_DEBUG == 1
  shellui_log("homeui_top_nav_patch: activated OnionHEN top-nav slot "
              "profile='%s' "
              "(hbc_base=0x%llx)",
              profile->name, (unsigned long long)hbc.base_offset);
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
