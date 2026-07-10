/* Copyright (C) 2025 OrionHEN / LightningMods */

#include "toolbox_route.hpp"

namespace toolbox {

RouteResult resolve_resource(const RouteInput &in) {
  RouteResult out{};

  if (in.resource == kOgDebugXml) {
    out.page = Page::RedirectOgDebug;
    return out;
  }

  out.flags.is_payloads = (in.resource == in.names.payloads_xml);
  out.flags.is_debug_settings = (in.resource == in.names.debug_settings_xml);
  out.flags.is_cheats = (in.resource == in.names.cheats_xml);
  out.flags.is_auto_payload = (in.resource == kAutoPayloadsXml);
  out.flags.is_plapps = (in.resource == kPlappsXml);
  out.flags.is_su_menu = (in.resource == kSuperuserXml);
  out.flags.is_remote_play = (in.resource == in.names.remote_play_xml);

  if (in.cheats_shortcut || in.cheats_shortcut_not_open) {
    out.flags.is_debug_settings = false;
    out.flags.is_cheats = true;
    out.shortcut_forced_cheats = true;
  }

  if (out.flags.is_debug_settings) {
    out.page = Page::DebugSettings;
  } else if (out.flags.is_payloads) {
    out.page = Page::Payloads;
  } else if (out.flags.is_cheats) {
    out.page = Page::Cheats;
    out.clear_cheat_shortcuts_after = true;
  } else if (out.flags.is_auto_payload) {
    out.page = Page::AutoPayloads;
  } else if (out.flags.is_remote_play) {
    out.page = Page::RemotePlay;
  } else if (out.flags.is_plapps) {
    out.page = Page::Plapps;
  } else if (out.flags.is_su_menu) {
    out.page = Page::SuperuserPass;
  } else {
    out.page = Page::None;
  }

  return out;
}

} // namespace toolbox
