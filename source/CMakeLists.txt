cmake_minimum_required (VERSION 3.20)

project("OrionHEN")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(ORIONHEN_ROOT "${PROJECT_ROOT}/.." ABSOLUTE)

# ---------------------------------------------------------------------------
# Artifact layout
#   ${CMAKE_BINARY_DIR}/bin  — ELFs (daemon, util, bootstrapper, OrionHEN, …)
#   ${CMAKE_BINARY_DIR}/lib  — built static libs (liborion_*, …)
# Prebuilt vendor .a stay in ${PROJECT_ROOT}/lib and are also on the link path.
#
# Default scripts/build.sh uses -B <repo>/build so products land at repo/build/.
# ---------------------------------------------------------------------------
set(ORIONHEN_OUT_BIN "${CMAKE_BINARY_DIR}/bin" CACHE PATH "ELF output directory")
set(ORIONHEN_OUT_LIB "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Built static-lib output directory")

file(MAKE_DIRECTORY "${ORIONHEN_OUT_BIN}" "${ORIONHEN_OUT_LIB}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ORIONHEN_OUT_BIN})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ORIONHEN_OUT_LIB})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ORIONHEN_OUT_LIB})

# .incbin paths under source/ still use ../../bin/... relative to source files.
# Keep a symlink source/bin → ${ORIONHEN_OUT_BIN} so those paths resolve.
set(_orion_bin_link "${PROJECT_ROOT}/bin")
if(EXISTS "${_orion_bin_link}" AND NOT IS_SYMLINK "${_orion_bin_link}")
	message(WARNING
		"OrionHEN: ${_orion_bin_link} exists as a real directory; "
		"prefer removing it so a symlink to ${ORIONHEN_OUT_BIN} can be created.")
else()
	file(REMOVE "${_orion_bin_link}")
	file(CREATE_LINK "${ORIONHEN_OUT_BIN}" "${_orion_bin_link}" SYMBOLIC)
endif()

include_directories	(SYSTEM "${PROJECT_ROOT}/include")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}/include")

# Built libs first, then vendored prebuilts under source/lib/.
link_directories("${ORIONHEN_OUT_LIB}" "${PROJECT_ROOT}/lib")

add_subdirectory(libhijacker)
add_subdirectory(libNidResolver)
# add_subdirectory(libRPI)
# libSelfDecryptor removed from OrionHEN (FTP uses local decrypt_self only)
# Shared proc/ucred before NineS (inject path links these symbols).
add_subdirectory(liborion_proc)
add_subdirectory(liborion_platform)
add_subdirectory(liborion_payload)
add_subdirectory(liborion_ipc)
add_subdirectory(liborion_settings)
add_subdirectory(liborion_detour)
add_subdirectory(liborion_ready)
add_subdirectory(liborion_playtime)
# Shared ptrace + ELF inject (authid elevated once at inject entry, not per syscall).
add_subdirectory(liborion_elfldr)
add_subdirectory(libNineS)
add_subdirectory(shellui)
add_subdirectory(fps_elf)
add_subdirectory(util)
add_subdirectory(daemon)
add_subdirectory(bootstrapper)
add_subdirectory(unpacker)
