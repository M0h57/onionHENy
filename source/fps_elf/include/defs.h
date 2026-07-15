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

#pragma once
#include <stdint.h>
#include <stddef.h>

/*
 * sceKernelMprotect is provided as a *function pointer* (external_symbols.cpp)
 * and filled via sceKernelDlsym — same shape as shellui. Do NOT declare a
 * real function here: DetourFunction calls through that pointer.
 */

#define libSceKernelHandle 0x2001
#define KERNEL_DLSYM(handle, sym) \
    (*(void**)&sym=(void*)kernel_dynlib_dlsym(-1, handle, #sym))


typedef void* ScePthread;

