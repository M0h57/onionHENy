###################################################################################################
# OnionHEN shared Detour + hde64 (shellui / fps_elf injectees)
###################################################################################################

cmake_minimum_required(VERSION 3.20)

set(basename "onion_detour")
project(${basename} C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(IS_SUBPROJECT TRUE)
else()
  set(IS_SUBPROJECT FALSE)
endif()

message("IS_SUBPROJECT: ${IS_SUBPROJECT}")

set(D_CWD "${PROJECT_SOURCE_DIR}")

include_directories(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}/include")

add_library(${PROJECT_NAME} STATIC
  source/detour.cpp
  source/hde64.cpp
  source/hotpatch.cpp
  source/x64_relocator.cpp
)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

if(NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "[Cc]lang")
  message(FATAL_ERROR "${PROJECT_NAME} is meant to be built with clang! CompilerID: ${CMAKE_CXX_COMPILER_ID}")
endif()

message("========== build: ${PROJECT_NAME} ==========")

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -march=znver2 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -fstack-protector-all")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -O2 -Wall -Wextra -Werror -fno-exceptions")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g0 -O2")

target_include_directories(${PROJECT_NAME}
  PUBLIC  "${D_CWD}/include"
  PRIVATE "${D_CWD}/../include"
  PRIVATE "${D_CWD}/../libonion_ipc/include"
)

# Logging symbols (shellui_log) come from onion_ipc at final link.
add_dependencies(${PROJECT_NAME} onion_ipc)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
