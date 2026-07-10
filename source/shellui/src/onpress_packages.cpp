/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress custom PKG / payload apps */
#include "onpress.hpp"
#include "external_symbols.hpp"
#include <msg.hpp>

static OnPressResult prefix_id_pkg(OnPressContext &ctx) {
  if (ctx.id.rfind("id_pkg_", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (custom_pkg_list.empty()) {
    return OnPressResult::EarlyReturn;
  }
  for (auto selected_pkgs : custom_pkg_list) {
    if (selected_pkgs.id != ctx.id) {
      continue;
    }
#if SHELL_DEBUG == 1
    shellui_log("[Clicked %s] %s path: %s", selected_pkgs.id.c_str(),
                selected_pkgs.name.c_str(), selected_pkgs.shellui_path.c_str());
#endif
    // Prefer shellui-visible path for /data; otherwise install by path.
    std::string dl_url =
        (selected_pkgs.path.rfind("/data") != std::string::npos)
            ? selected_pkgs.shellui_path
            : selected_pkgs.path;

    playgo_info_t playgoinfo = {};
    pkg_info_t pkginfo = {};
    pkg_metadata_t metainfo;
    metainfo.playgo_scenario_id = "";
    metainfo.content_name = "";
    metainfo.content_id = "";
    metainfo.icon_url = "";
    metainfo.ex_uri = "";
    metainfo.uri = dl_url.c_str();

    shellui_log("Installing package from: %s", metainfo.uri);
    int num = sceAppInstUtilInstallByPackage(&metainfo, &pkginfo, &playgoinfo);
    if (num != 0) {
      notify("Failed to install %s\nError: 0x%X", selected_pkgs.name.c_str(), num);
    } else {
      notify("%s installation started successfully", selected_pkgs.name.c_str());
    }
  }
  return OnPressResult::Handled;
}

static OnPressResult prefix_id_pl_loader(OnPressContext &ctx) {
  if (ctx.id.rfind("id_orionhen_pl_loader_", 0) != 0) {
    return OnPressResult::NotMine;
  }
  if (games_list.empty()) {
    return OnPressResult::EarlyReturn;
  }
  for (const auto &game : games_list) {
    if (game.id == ctx.id) {
      break;
    }
  }
  return OnPressResult::Handled;
}

static const OnPressPrefixEntry kPrefix[] = {
    {"id_pkg_", prefix_id_pkg},
    {"id_orionhen_pl_loader_", prefix_id_pl_loader},
};

const OnPressPrefixEntry *onpress_packages_prefix(size_t *count) {
  *count = sizeof(kPrefix) / sizeof(kPrefix[0]);
  return kPrefix;
}
