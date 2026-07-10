###################################################################################################
# OrionHEN shared ptrace + ELF inject primitives (single authid policy).
###################################################################################################

cmake_minimum_required(VERSION 3.20)

set(basename "orion_elfldr")
project(${basename} C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(D_CWD "${PROJECT_SOURCE_DIR}")

include_directories(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}/include")

add_library(${PROJECT_NAME} STATIC
  source/pt.c
  source/elfldr.c
)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

message("========== build: ${PROJECT_NAME} ==========")

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -march=znver2 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} ")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200112 -D__BSD_VISIBLE=1 -D__XSI_VISIBLE=500")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-builtin -nostdlib -Wall -fPIC -gfull -O3")

target_include_directories(${PROJECT_NAME}
  PUBLIC  "${D_CWD}/include"
  PRIVATE "${D_CWD}/../include"
)

# Authid policy: no per-syscall flip in pt.c; injectors elevate once at entry.
target_compile_definitions(${PROJECT_NAME} PRIVATE ORION_ELFLDR_NO_PER_CALL_AUTHID=1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
