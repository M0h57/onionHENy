/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Title-ID policy for the sandbox-file app-jailbreak protocol.
 */
#pragma once

#include <array>
#include <string_view>

namespace onion::app_jailbreak {

inline constexpr std::array<std::string_view, 5> kExactTitleIds = {
    "ITEM00001", // Itemzflow loader uses download0/etahen_jailbreak
    "NPXS39041",
    "PKGI13337",
    "PKGI12345",
    "TOOL00001",
};

constexpr bool is_whitelisted(std::string_view tid) {
  for (const std::string_view allowed : kExactTitleIds) {
    if (tid == allowed) {
      return true;
    }
  }
  return tid.find("LAPY") != std::string_view::npos;
}

constexpr const char *whitelist_reason(std::string_view tid) {
  for (const std::string_view allowed : kExactTitleIds) {
    if (tid == allowed) {
      return "exact";
    }
  }
  if (tid.find("LAPY") != std::string_view::npos) {
    return "LAPY*";
  }
  return "none";
}

} // namespace onion::app_jailbreak
