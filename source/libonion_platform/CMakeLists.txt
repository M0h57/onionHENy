###################################################################################################
# OnionHEN platform leaves: fs / log / notify
###################################################################################################

cmake_minimum_required(VERSION 3.20)

set(basename "onion_platform")
project(${basename} C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(D_CWD "${PROJECT_SOURCE_DIR}")

include_directories(SYSTEM "${PS5_PAYLOAD_SDK}")
include_directories(SYSTEM "${PS5_PAYLOAD_SDK}/include")

add_library(${PROJECT_NAME} STATIC
  source/fs.c
  source/log.c
  source/notify.c
  source/notify_i18n.c
)
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

message("========== build: ${PROJECT_NAME} ==========")

set(CMAKE_C_FLAGS "--target=x86_64-sie-ps5 -march=znver2 -DPPR -DPS5 -DPS5_FW_VERSION=${V_FW} -fPIC -fstack-protector-all -O2 -Wall -Wextra -Werror")

target_include_directories(${PROJECT_NAME}
  PUBLIC  "${D_CWD}/include"
  PRIVATE "${D_CWD}/../include"
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
