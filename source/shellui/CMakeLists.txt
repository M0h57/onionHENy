cmake_minimum_required (VERSION 3.20)

set(basename "shellui")
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
else()
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${D_CWD}/../bin)
endif()
if(DEFINED ONIONHEN_OUT_LIB)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${ONIONHEN_OUT_LIB})
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${ONIONHEN_OUT_LIB})
else()
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${D_CWD}/bin)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${D_CWD}/bin)
endif()
set(D_SRC ${D_CWD}/src)

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

# A/B: disable HomeUI top-nav Hermes patch without editing sources.
#   cmake -DONIONHEN_SHELLUI_HOMEUI_TOP_NAV_PATCH=OFF ...
option(ONIONHEN_SHELLUI_HOMEUI_TOP_NAV_PATCH
	"Patch HomeUI top-nav Fps slot into OnionHEN entry" ON)
if(NOT ONIONHEN_SHELLUI_HOMEUI_TOP_NAV_PATCH)
	target_compile_definitions(${PROJECT_NAME} PRIVATE SHELLUI_HOMEUI_TOP_NAV_PATCH=0)
	message(STATUS "shellui: HomeUI top-nav patch DISABLED")
endif()

message("========== build: ${PROJECT_NAME} ==========")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(I18N_DIR "${D_CWD}/i18n")
set(I18N_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(I18N_CATALOG "${I18N_GENERATED_DIR}/toolbox_i18n_catalog.inc")
add_custom_command(
	OUTPUT "${I18N_CATALOG}"
	COMMAND "${Python3_EXECUTABLE}" "${I18N_DIR}/generate_catalog.py"
		--zh-cn "${I18N_DIR}/zh-CN.json"
		--en-us "${I18N_DIR}/en-US.json"
		--output "${I18N_CATALOG}"
	DEPENDS
		"${I18N_DIR}/generate_catalog.py"
		"${I18N_DIR}/zh-CN.json"
		"${I18N_DIR}/en-US.json"
	COMMENT "Generating toolbox i18n catalog"
	VERBATIM
)
add_custom_target(shellui_i18n_catalog DEPENDS "${I18N_CATALOG}")

file(GLOB SrcFiles ${D_SRC}/*.c ${D_SRC}/*.cpp ${D_SRC}/../include/*.h ${D_SRC}/../include/*.hpp ${D_SRC}/*.s ${ONIONHEN_THIRD_PARTY_DIR}/cjson/*.cpp ${ONIONHEN_THIRD_PARTY_DIR}/cjson/*.hpp ${ONIONHEN_COMMON_DIR}/backtrace.cpp)

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-nested-anon-types -Wno-gnu-anonymous-struct -Wno-zero-length-array -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -gfull -gdwarf-2 -O0 -pedantic -pedantic-errors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 ")

target_sources(${PROJECT_NAME} PRIVATE ${SrcFiles})
add_dependencies(${PROJECT_NAME} shellui_i18n_catalog onion_version onion_ipc onion_settings onion_detour onion_proc onion_platform onion_ready)
target_include_directories(${PROJECT_NAME} PRIVATE
	"${D_CWD}/include"
	"${I18N_GENERATED_DIR}"
	"${ONIONHEN_THIRD_PARTY_DIR}/cjson"
	"${PROJECT_ROOT}/libonion_ipc/include"
	"${PROJECT_ROOT}/libonion_settings/include"
	"${PROJECT_ROOT}/libonion_detour/include"
	"${PROJECT_ROOT}/libonion_proc/include"
	"${PROJECT_ROOT}/libonion_platform/include"
	"${PROJECT_ROOT}/include"
)
target_link_directories	(${PROJECT_NAME} PUBLIC "${ONIONHEN_PS5_STUB_DIR}")
target_link_libraries	(${PROJECT_NAME} PUBLIC onion_detour onion_proc onion_platform onion_ipc onion_settings onion_ready SceRegMgr SceSystemService SceUserService SceNetCtl SceAppInstUtil)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add post-build command
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section .debug_info --remove-section .debug_abbrev --remove-section .debug_line --remove-section .debug_str --remove-section .debug_loc --remove-section .debug_aranges --remove-section .debug_ranges --remove-section .debug_pubnames --remove-section .debug_pubtypes --remove-section .debug_frame --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping debugging information from ${PROJECT_NAME}.elf"
)
