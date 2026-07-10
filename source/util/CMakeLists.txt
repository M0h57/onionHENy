cmake_minimum_required (VERSION 3.20)

set(basename "util")

project(${basename} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

if (NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	set(IS_SUBPROJECT TRUE)
else()
	set(IS_SUBPROJECT FALSE)
endif()

message("IS_SUBPROJECT: ${IS_SUBPROJECT}")

set(D_CWD "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${D_CWD}/../bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${D_CWD}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${D_CWD}/bin)
set(D_SRC ${D_CWD}/source)

include_directories	(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories	(SYSTEM "${PS5_PAYLOAD_SDK}/include")
include_directories (SYSTEM "${D_CWD}/../libelfloader/include")

add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.elf")

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "[Cc]lang")
	set(IS_CLANG 1)
else()
	message(FATAL_ERROR "${PROJECT_NAME} is meant to be built with clang! CompilerID: ${CMAKE_CXX_COMPILER_ID}")
endif()

target_compile_options(${PROJECT_NAME} PUBLIC
	$<$<COMPILE_LANGUAGE:C>:${C_DEFS} ${C_FLAGS}>
	$<$<COMPILE_LANGUAGE:CXX>:${CXX_DEFS} ${CXX_FLAGS}>
	$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAGS}>
)

message("========== build: ${PROJECT_NAME} ==========")

file(GLOB SrcFiles
  ${D_SRC}/*.c
  ${D_SRC}/*.cpp
  ${D_SRC}/cheats/*.c
  ${D_SRC}/cheats/*.cpp
  ${D_SRC}/cheats/third_party/*.c
  ${D_SRC}/../include/*.h
  ${D_SRC}/../include/*.hpp
  ${D_SRC}/*.s
  ${D_SRC}/../../extern/cJSON/*.cpp
  ${D_SRC}/../../extern/cJSON/*.hpp
  ${D_SRC}/../../lib/backtrace.cpp
)

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all -Wno-unused-const-variable -Wno-deprecated-declarations")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -gfull -gdwarf-2 -O0 -pedantic -pedantic-errors")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

target_sources(${PROJECT_NAME} PRIVATE ${SrcFiles} "${PROJECT_ROOT}/lib/elfldr_remote.c")
target_include_directories(${PROJECT_NAME} PRIVATE
  "${D_CWD}/include"
  "${PROJECT_ROOT}/liborion_settings/include"
  "${PROJECT_ROOT}/liborion_ipc/include"
  "${PROJECT_ROOT}/include"
)
# CBC for MC4 / ShnExt AES; no KCF/WMDW
target_compile_definitions(${PROJECT_NAME} PRIVATE CBC=1)
target_link_directories(${PROJECT_NAME} PUBLIC
  "${PROJECT_ROOT}/lib"
  "${D_CWD}/lib"
  "${D_CWD}/lib/keystone_cxxrt"
)
add_dependencies(${PROJECT_NAME} hijacker NineS orion_settings orion_ipc orion_ready)
# keystone: ShnExt ASM only
target_link_libraries(${PROJECT_NAME} PUBLIC
    orion_ipc
    orion_settings
    orion_ready
    keystone
    c++
    c++abi
    unwind
    hijacker
    SceLibcInternal
    SceSystemService
    SceNet
    SceSysmodule
    SceUserService
    SceNetCtl
    SceSysCore
    ScePad
    SceVideoOut
    kernel_sys
    SceAppInstUtil
    NineS
    SceHttp2
    SceSsl
    z
    curl
    minizip
    microhttpd
    psl
    zstd
    mbedtls
    mbedx509
    mbedcrypto
    wolfssl
)

set_source_files_properties(
  ${D_SRC}/cheats/third_party/aes.c
  ${D_SRC}/cheats/third_party/base64.c
  ${D_SRC}/cheats/third_party/miniz.c
  ${D_SRC}/cheats/third_party/sha256.c
  PROPERTIES COMPILE_FLAGS "-Wno-error -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter"
)
file(GLOB CHEAT_ENGINE_SRCS ${D_SRC}/cheats/*.c)
set_source_files_properties(${CHEAT_ENGINE_SRCS} PROPERTIES
  COMPILE_FLAGS "-Wno-error -Wno-unused-parameter -Wno-sign-compare"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section .debug_info --remove-section .debug_abbrev --remove-section .debug_line --remove-section .debug_str --remove-section .debug_loc --remove-section .debug_aranges --remove-section .debug_ranges --remove-section .debug_pubnames --remove-section .debug_pubtypes --remove-section .debug_frame --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping debugging information from ${PROJECT_NAME}.elf"
)
