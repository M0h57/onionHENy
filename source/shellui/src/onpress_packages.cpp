/* Copyright (C) 2025 OnionHEN / LightningMods — OnPress packages / payload apps */
#include "onpress.hpp"
#include "external_symbols.hpp"

#include <cstring>

extern "C" int sceAppInstUtilInitialize(void);

namespace {

bool ends_with_ci(const std::string &s, const char *suffix) {
  const size_t n = std::strlen(suffix);
  if (s.size() < n)
    return false;
  for (size_t i = 0; i < n; ++i) {
    unsigned char a = static_cast<unsigned char>(s[s.size() - n + i]);
    unsigned char b = static_cast<unsigned char>(suffix[i]);
    if (a >= 'A' && a <= 'Z')
      a = static_cast<unsigned char>(a + 32);
    if (b >= 'A' && b <= 'Z')
      b = static_cast<unsigned char>(b + 32);
    if (a != b)
      return false;
  }
  return true;
}

/** Stock PkgInstaller rows use absolute path / URI as Id (no Value). */
bool is_pkg_install_id(const std::string &id) {
  if (!ends_with_ci(id, ".pkg"))
    return false;
  return !id.empty() &&
         (id[0] == '/' || id.rfind("file:", 0) == 0 ||
          id.rfind("http://", 0) == 0 || id.rfind("https://", 0) == 0);
}

} // namespace

OnPressResult onpress_try_pkg_path(OnPressContext &ctx) {
  if (!is_pkg_install_id(ctx.id))
    return OnPressResult::NotMine;

  playgo_info_t playgoinfo{};
  pkg_info_t pkginfo{};
  pkg_metadata_t metainfo{};
  metainfo.uri = ctx.id.c_str();
  metainfo.ex_uri = "";
  metainfo.playgo_scenario_id = "";
  metainfo.content_id = "";
  metainfo.content_name = "";
  metainfo.icon_url = "";

  shellui_log("PkgInstaller: installing %s", ctx.id.c_str());
  (void)sceAppInstUtilInitialize();
  const int rc =
      sceAppInstUtilInstallByPackage(&metainfo, &pkginfo, &playgoinfo);
  if (rc != 0) {
    notify("Failed to install package\nError: 0x%X", rc);
  } else {
    notify("Installation started:\n%s", ctx.id.c_str());
  }
  ctx.dirty = false;
  return OnPressResult::Consumed;
}

static OnPressResult prefix_id_pl_loader(OnPressContext &ctx) {
  if (ctx.id.rfind("id_onionhen_pl_loader_", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (g_ui.games_list.empty()) {
    return OnPressResult::EarlyReturn;
  }
  for (const auto &game : g_ui.games_list) {
    if (game.id == ctx.id) {
      break;
    }
  }
  return OnPressResult::Handled;
}

static const OnPressPrefixEntry kPrefix[] = {
    {"id_onionhen_pl_loader_", prefix_id_pl_loader},
};

const OnPressPrefixEntry *onpress_packages_prefix(size_t *count) {
  *count = sizeof(kPrefix) / sizeof(kPrefix[0]);
  return kPrefix;
}
