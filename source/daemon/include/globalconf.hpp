/* Copyright (C) 2025 OrionHEN / LightningMods

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

#pragma once

// Daemon product config surface only.
// SDK / pad / video-out / net typedefs do NOT belong here — use the payload
// SDK headers or local decls in the .cpp that needs them.

#include <orion/settings.hpp>

enum StartOpts {
  NONE = 0,
  HOME_MENU,
  SETTINGS,
  TOOLBOX,
};

// Thread-safe process store (same schema as util / shellui).
// Readers: g_settings.snapshot().  Writers: g_settings.store / update.
extern orion::SettingsStore g_settings;

int launchApp(const char *titleId);
