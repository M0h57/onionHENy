/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Lightweight i18n for dynamic toolbox XML (zh-Hans / en).
 */
#pragma once

#include <string>
#include <string_view>

namespace toolbox_i18n {

/** 0 = Simplified Chinese (default), 1 = English. Matches Settings.ui_lang. */
enum class Lang : int {
  ZhHans = 0,
  En = 1,
};

/** Active language for tr() (from settings or explicit set). */
Lang active_lang();

/** Active language as Settings.ui_lang-compatible integer. */
int active_ui_lang_value();

/**
 * Apply Settings.ui_lang (0=zh-Hans, 1=en). Call after LoadSettings /
 * before building toolbox XML. Invalid values fall back to zh-Hans.
 */
void apply_ui_lang(int ui_lang);

/**
 * Prefer the PS5 system language for toolbox XML; fall back to Settings.ui_lang
 * when SystemService is unavailable (host tests / unresolved symbol).
 */
void apply_system_or_ui_lang(int ui_lang);

/** Override without persisting (tests / temporary). */
void set_lang(Lang lang);

/**
 * Look up a UI string by stable key.
 * Missing key returns the key itself (visible failure).
 */
const char *tr(const char *key);

/** Convenience: tr(key) as std::string. */
inline std::string trs(const char *key) { return tr(key); }

} // namespace toolbox_i18n
