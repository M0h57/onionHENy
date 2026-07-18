cmake_minimum_required(VERSION 3.20)

set(basename "onion_elfldr_server")
project(${basename} C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

include_directories(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}/include")

add_executable(${PROJECT_NAME} source/main.c)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "onion_elfldr.elf")

message("========== build: ${PROJECT_NAME} ==========")

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112 -D__BSD_VISIBLE=1 -D__XSI_VISIBLE=500")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-keyword-macro -fPIC -fPIE -march=znver2 -Wall -Werror -fstack-protector-all -O2")

target_include_directories(${PROJECT_NAME} PRIVATE
  "${PROJECT_ROOT}/include"
  "${PROJECT_ROOT}/libonion_elfldr/include"
)
target_link_directories(${PROJECT_NAME} PUBLIC "${ONIONHEN_PS5_STUB_DIR}")
add_dependencies(${PROJECT_NAME} onion_elfldr)
target_link_libraries(${PROJECT_NAME} PUBLIC onion_elfldr)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} --remove-section .debug_info --remove-section .debug_abbrev --remove-section .debug_line --remove-section .debug_str --remove-section .debug_loc --remove-section .debug_aranges --remove-section .debug_ranges --remove-section .debug_pubnames --remove-section .debug_pubtypes --remove-section .debug_frame --strip-unneeded $<TARGET_FILE:${PROJECT_NAME}>
  COMMENT "Stripping debugging information from onion_elfldr.elf"
)
