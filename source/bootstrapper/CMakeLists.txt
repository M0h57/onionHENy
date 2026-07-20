cmake_minimum_required (VERSION 3.20)

set(basename "bootstrapper")
project(${basename} C CXX ASM)

# Language Standard Defaults
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

# Check for sub-project as part of main build or external build
if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(IS_SUBPROJECT TRUE)
else()
	set(IS_SUBPROJECT FALSE)
endif()

message("IS_SUBPROJECT: ${IS_SUBPROJECT}")

set(D_CWD "${CMAKE_CURRENT_SOURCE_DIR}")
if(DEFINED ONIONHEN_OUT_BIN)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${ONIONHEN_OUT_BIN})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ONIONHEN_OUT_LIB})
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ONIONHEN_OUT_LIB})
else()
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${D_CWD}/../bin)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${D_CWD}/bin)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${D_CWD}/bin)
endif()
set(D_SRC ${D_CWD}/source)

# Headers
include_directories	(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories	(SYSTEM "${PS5_PAYLOAD_SDK}/include")

add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.elf")

# Must build with clang
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "[Cc]lang")
	set(IS_CLANG 1)
else()
	message(FATAL_ERROR "${PROJECT_NAME} is meant to be built with clang! CompilerID: ${CMAKE_CXX_COMPILER_ID}")
endif()

# Finalize main target sources
target_compile_options(${PROJECT_NAME} PUBLIC
	$<$<COMPILE_LANGUAGE:C>:${C_DEFS} ${C_FLAGS}>
	$<$<COMPILE_LANGUAGE:CXX>:${CXX_DEFS} ${CXX_FLAGS}>
	$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAGS}>
)

message("========== build: ${PROJECT_NAME} ==========")


# Byepervisor removed from OnionHEN — bootstrapper sources only.
# pt.c / elfldr.c live in libonion_elfldr (raise_privileges + inject primitives).
file(GLOB SrcFiles ${D_SRC}/*.c ${D_SRC}/*.cpp ${D_SRC}/../include/*.h ${D_SRC}/../include/*.hpp ${D_SRC}/*.s ${ONIONHEN_COMMON_DIR}/backtrace.cpp)
list(FILTER SrcFiles EXCLUDE REGEX ".*/(pt|elfldr)\\.c$")

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -gfull -gdwarf-2 -O0 -pedantic -pedantic-errors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

target_sources(${PROJECT_NAME} PRIVATE ${SrcFiles})
# .incbin in daemon.c is invisible to the compiler dependency scanner.
# Without OBJECT_DEPENDS, daemon/util/kstuff rebuilds do not re-embed into bootstrapper.
if(DEFINED ONIONHEN_OUT_BIN)
	set(_onion_daemon_elf "${ONIONHEN_OUT_BIN}/daemon.elf")
	set(_onion_util_elf "${ONIONHEN_OUT_BIN}/util.elf")
	set(_onion_elfldr_elf "${ONIONHEN_OUT_BIN}/onion_elfldr.elf")
else()
	set(_onion_daemon_elf "${PROJECT_ROOT}/bin/daemon.elf")
	set(_onion_util_elf "${PROJECT_ROOT}/bin/util.elf")
	set(_onion_elfldr_elf "${PROJECT_ROOT}/bin/onion_elfldr.elf")
endif()
set_source_files_properties(
	${D_SRC}/daemon.c
	PROPERTIES OBJECT_DEPENDS
		"${_onion_daemon_elf};${_onion_util_elf};${_onion_elfldr_elf};${ONIONHEN_KSTUFF_ELF};${D_CWD}/assets/onionhen_sicon.png;${D_CWD}/assets/icon_xml_package.png;${D_CWD}/assets/icon_xml_plugins.png;${D_CWD}/assets/icon_xml_game.png;${D_CWD}/assets/icon_xml_monitor.png;${D_CWD}/assets/icon_xml_account.png;${D_CWD}/assets/icon_xml_network.png;${D_CWD}/assets/icon_xml_settings.png;${D_CWD}/assets/icon_xml_shortcuts.png;${D_CWD}/assets/icon_xml_debug.png;${D_CWD}/assets/icon_xml_about.png;${D_CWD}/assets/icon_xml_kstuff.png;${D_CWD}/assets/icon_xml_overlay.png;${D_CWD}/assets/icon_xml_fan.png;${D_CWD}/assets/icon_xml_restmode.png;${D_CWD}/assets/icon_xml_donations.png;${D_CWD}/assets/icon_xml_credits.png;${D_CWD}/assets/icon_xml_project.png"
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
	ONIONHEN_DAEMON_ELF="${_onion_daemon_elf}"
	ONIONHEN_UTIL_ELF="${_onion_util_elf}"
	ONIONHEN_ELFLDR_ELF="${_onion_elfldr_elf}"
	ONIONHEN_KSTUFF_ELF="${ONIONHEN_KSTUFF_ELF}"
)
# elfldr_raise_privileges from libonion_elfldr; launches go via elfldr_remote.c.
target_include_directories(${PROJECT_NAME} PRIVATE
	"${D_CWD}/include"
	"${PROJECT_ROOT}/libonion_proc/include"
	"${PROJECT_ROOT}/libonion_platform/include"
	"${PROJECT_ROOT}/libonion_payload/include"
	"${PROJECT_ROOT}/libonion_elfldr/include"
	"${PROJECT_ROOT}/include"
)
target_link_directories	(${PROJECT_NAME} PUBLIC "${ONIONHEN_PS5_STUB_DIR}")
target_sources(${PROJECT_NAME} PRIVATE "${ONIONHEN_COMMON_DIR}/elfldr_remote.c")
target_link_libraries	(${PROJECT_NAME} PUBLIC onion_proc onion_platform onion_payload onion_ready onion_elfldr SceNotification)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_dependencies(${PROJECT_NAME} daemon util shellui onion_elfldr_server onion_proc onion_platform onion_payload onion_ready onion_elfldr)

# Copy bootstrapper.elf as test.elf before processing
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test.elf
    COMMENT "Copying ${PROJECT_NAME}.elf to test.elf"
)

# Add post-build command
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section .debug_info --remove-section .debug_abbrev --remove-section .debug_line --remove-section .debug_str --remove-section .debug_loc --remove-section .debug_aranges --remove-section .debug_ranges --remove-section .debug_pubnames --remove-section .debug_pubtypes --remove-section .debug_frame --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping debugging information from ${PROJECT_NAME}.elf"
)

# Portable size + lzma pack (Linux GNU stat / macOS BSD stat; lzma or xz)
# PROJECT_ROOT is source/; pack script lives at repo scripts/
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E env "TARGET_ELF=$<TARGET_FILE:${PROJECT_NAME}>"
            ${PROJECT_ROOT}/../scripts/pack_bootstrapper.sh
    COMMENT "Recording size and compressing bootstrapper with lzma"
)
