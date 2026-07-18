cmake_minimum_required (VERSION 3.20)

set(basename "unpacker")
project(${basename} C ASM)

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


set(PROJECT_NAME OnionHEN)
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



set(LZMA_ROOT "${ONIONHEN_THIRD_PARTY_DIR}/7zip_sdk/C")
set(LZMA_LIB ${LZMA_ROOT}/LzmaLib.c ${LZMA_ROOT}/Alloc.c
				${LZMA_ROOT}/LzmaEnc.c
				${LZMA_ROOT}/LzFind.c
				${LZMA_ROOT}/LzFindMt.c
				${LZMA_ROOT}/CpuArch.c
				${LZMA_ROOT}/LzmaDec.c
				${LZMA_ROOT}/Threads.c
				${LZMA_ROOT}/LzFindOpt.c)

# main.c only — initial bootstrap is sent to external elfldr 9021.
set(SrcFiles ${D_SRC}/main.c ${LZMA_LIB})

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DDEBUG -O0")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

target_sources(${PROJECT_NAME} PRIVATE ${SrcFiles})
set_source_files_properties(
	${D_SRC}/main.c
	PROPERTIES OBJECT_DEPENDS
		"${ONIONHEN_OUT_BIN}/bootstrapper.elf.lzma;${ONIONHEN_OUT_BIN}/bootstrapper.elf.lzma.size"
)
target_include_directories(${PROJECT_NAME} PRIVATE
	"${D_CWD}/include"
	"${LZMA_ROOT}"
	"${PROJECT_ROOT}/libonion_platform/include"
)
target_link_directories(${PROJECT_NAME} PUBLIC "${ONIONHEN_PS5_STUB_DIR}")
target_link_libraries(${PROJECT_NAME} PUBLIC onion_platform SceNotification)
target_compile_definitions(${PROJECT_NAME} PRIVATE
	ONIONHEN_BOOTSTRAPPER_LZMA="${ONIONHEN_OUT_BIN}/bootstrapper.elf.lzma"
	ONIONHEN_BOOTSTRAPPER_SIZE="${ONIONHEN_OUT_BIN}/bootstrapper.elf.lzma.size"
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_dependencies(${PROJECT_NAME} bootstrapper onion_platform)

# Add post-build command
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section .debug_info --remove-section .debug_abbrev --remove-section .debug_line --remove-section .debug_str --remove-section .debug_loc --remove-section .debug_aranges --remove-section .debug_ranges --remove-section .debug_pubnames --remove-section .debug_pubtypes --remove-section .debug_frame --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Stripping debugging information from ${PROJECT_NAME}.elf"
)
