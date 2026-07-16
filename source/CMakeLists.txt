cmake_minimum_required (VERSION 3.20)

project("OnionHEN")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(ONIONHEN_ROOT "${PROJECT_ROOT}/.." ABSOLUTE)

# ---------------------------------------------------------------------------
# Artifact layout
#   ${CMAKE_BINARY_DIR}/bin  — ELFs (daemon, util, bootstrapper, OnionHEN, …)
#   ${CMAKE_BINARY_DIR}/lib  — built static libs (libonion_*, …)
# Prebuilt vendor .a stay in ${PROJECT_ROOT}/lib and are also on the link path.
#
# Default scripts/build.sh uses -B <repo>/build so products land at repo/build/.
# ---------------------------------------------------------------------------
set(ONIONHEN_OUT_BIN "${CMAKE_BINARY_DIR}/bin" CACHE PATH "ELF output directory")
set(ONIONHEN_OUT_LIB "${CMAKE_BINARY_DIR}/lib" CACHE PATH "Built static-lib output directory")

file(MAKE_DIRECTORY "${ONIONHEN_OUT_BIN}" "${ONIONHEN_OUT_LIB}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ONIONHEN_OUT_BIN})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ONIONHEN_OUT_LIB})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ONIONHEN_OUT_LIB})

# .incbin paths under source/ still use ../../bin/... relative to source files.
# Keep a symlink source/bin → ${ONIONHEN_OUT_BIN} so those paths resolve.
set(_onion_bin_link "${PROJECT_ROOT}/bin")
if(EXISTS "${_onion_bin_link}" AND NOT IS_SYMLINK "${_onion_bin_link}")
	message(WARNING
		"OnionHEN: ${_onion_bin_link} exists as a real directory; "
		"prefer removing it so a symlink to ${ONIONHEN_OUT_BIN} can be created.")
else()
	file(REMOVE "${_onion_bin_link}")
	file(CREATE_LINK "${ONIONHEN_OUT_BIN}" "${_onion_bin_link}" SYMBOLIC)
endif()

include_directories	(SYSTEM "${PROJECT_ROOT}/include")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}/include")

# Built libs first, then vendored prebuilts under source/lib/.
link_directories("${ONIONHEN_OUT_LIB}" "${PROJECT_ROOT}/lib")

add_subdirectory(libhijacker)
add_subdirectory(libNidResolver)
# add_subdirectory(libRPI)
# libSelfDecryptor removed from OnionHEN (FTP uses local decrypt_self only)
# Shared proc/ucred before NineS (inject path links these symbols).
add_subdirectory(libonion_proc)
add_subdirectory(libonion_platform)
add_subdirectory(libonion_payload)
add_subdirectory(libonion_ipc)
add_subdirectory(libonion_settings)
add_subdirectory(libonion_detour)
add_subdirectory(libonion_ready)
# Shared ptrace + ELF inject (authid elevated once at inject entry, not per syscall).
add_subdirectory(libonion_elfldr)
add_subdirectory(libNineS)
add_subdirectory(shellui)
add_subdirectory(fps_elf)
add_subdirectory(util)
add_subdirectory(daemon)
add_subdirectory(bootstrapper)
add_subdirectory(unpacker)
