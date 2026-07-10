/* Copyright (C) 2025 OrionHEN / LightningMods — OnPress payload apps */
#include "onpress.hpp"

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
    {"id_orionhen_pl_loader_", prefix_id_pl_loader},
};

const OnPressPrefixEntry *onpress_packages_prefix(size_t *count) {
  *count = sizeof(kPrefix) / sizeof(kPrefix[0]);
  return kPrefix;
}
