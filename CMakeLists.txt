# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rm_test)

target_include_directories(app PRIVATE
  .
  app/algorithms
  ${HPM_SDK_DIR}/middleware/hpm_math/nds_dsp
)

target_link_libraries(app PRIVATE
  ${HPM_SDK_DIR}/middleware/hpm_math/nds_dsp/gcc/libdsp.a
)

if(CONFIG_CHERRYUSB)
  set(CHERRYUSB_CONFIG_DIR ${HPM_ZEPHYR_DIR}/samples/cherryusb/config)
  zephyr_include_directories(${CHERRYUSB_CONFIG_DIR})
  zephyr_include_directories(${HPM_SDK_DIR}/components/usb/device)
endif()

target_sources(app PRIVATE
  src/main.cpp
)

add_subdirectory(app)
add_subdirectory(platform)
