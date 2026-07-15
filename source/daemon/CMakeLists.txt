cmake_minimum_required (VERSION 3.20)

set(basename "daemon")
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
include_directories (SYSTEM "${D_CWD}/../libelfloader/include")

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


file(GLOB SrcFiles ${D_SRC}/*.c ${D_SRC}/*.cpp ${D_SRC}/../include/*.h ${D_SRC}/../include/*.hpp ${D_SRC}/*.s ${D_SRC}/../../extern/cJSON/*.cpp ${D_SRC}/../../extern/cJSON/*.hpp ${D_SRC}/../../lib/backtrace.cpp)

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -gfull -gdwarf-2 -O0 -pedantic -pedantic-errors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 ")

target_sources(${PROJECT_NAME} PRIVATE ${SrcFiles} "${PROJECT_ROOT}/lib/elfldr_remote.c")
# .incbin in embeddded_payloads.c is invisible to the compiler dependency scanner.
# Without OBJECT_DEPENDS, embedded payload rebuilds do not re-embed into daemon.elf.
if(DEFINED ONIONHEN_OUT_BIN)
	set(_onion_util_elf "${ONIONHEN_OUT_BIN}/util.elf")
else()
	set(_onion_util_elf "${PROJECT_ROOT}/bin/util.elf")
endif()
set_source_files_properties(
	${D_SRC}/embeddded_payloads.c
	PROPERTIES OBJECT_DEPENDS
		"${D_CWD}/assets/shellui.elf;${D_CWD}/assets/fps_elf.elf;${_onion_util_elf}"
)
target_include_directories(${PROJECT_NAME} PRIVATE
	"${D_CWD}/include"
	"${PROJECT_ROOT}/libonion_settings/include"
	"${PROJECT_ROOT}/libonion_ipc/include"
	"${PROJECT_ROOT}/libonion_proc/include"
	"${PROJECT_ROOT}/libonion_platform/include"
	"${PROJECT_ROOT}/include"
)
target_link_directories	(${PROJECT_NAME} PUBLIC "${PROJECT_ROOT}/lib")
add_dependencies(${PROJECT_NAME} util NineS shellui hijacker onion_settings onion_ipc onion_proc onion_platform onion_ready onion_playtime onion_elfldr)
# Restarts the embedded util bytes via external 9021 elfldr.
target_link_libraries	(${PROJECT_NAME} PUBLIC onion_ipc onion_settings onion_proc onion_platform onion_ready onion_playtime onion_elfldr hijacker ScePad SceSystemService SceNotification SceNet SceRegMgr SceSysmodule SceUserService SceNetCtl SceSysCore kernel_sys NineS)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Add post-build command
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section .debug_info --remove-section .debug_abbrev --remove-section .debug_line --remove-section .debug_str --remove-section .debug_loc --remove-section .debug_aranges --remove-section .debug_ranges --remove-section .debug_pubnames --remove-section .debug_pubtypes --remove-section .debug_frame --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping debugging information from ${PROJECT_NAME}.elf"
)
