# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rm_test)

target_include_directories(app PRIVATE
  .
  app/algorithms
)

if(CONFIG_CHERRYUSB)
  set(CHERRYUSB_CONFIG_DIR ${HPM_ZEPHYR_DIR}/samples/cherryusb/config)
  zephyr_include_directories(${CHERRYUSB_CONFIG_DIR})
  zephyr_include_directories(${HPM_SDK_DIR}/components/usb/device)
endif()

target_sources(app PRIVATE
  src/main.cpp
  app/src/app_main.cpp
  app/bootstrap/src/bootstrap.cpp
  app/bootstrap/src/module_manager.cpp
  app/services/actuator/actuator_service.cpp
  app/services/chassis/chassis_tuning_service.cpp
  app/services/runtime/runtime_init_service.cpp
  app/modules/modules_registry.cpp
  app/modules/chassis/chassis_module.cpp
  app/modules/remote_input/remote_input_module.cpp
  app/modules/arm/arm_module.cpp
  app/modules/gimbal/gimbal_module.cpp
  app/modules/gantry/gantry_module.cpp
  app/modules/referee/referee_module.cpp
  app/modules/rgb_led/rgb_led_module.cpp
  app/algorithms/control/alg_pid.cpp
  app/algorithms/math/alg_math.cpp
  app/channels/system_status_channel.cpp
  app/channels/chassis_command_channel.cpp
  app/channels/chassis_state_channel.cpp
  app/channels/remote_input_channel.cpp
  app/channels/motor_feedback_channel.cpp
  app/channels/arm_command_channel.cpp
  app/channels/arm_state_channel.cpp
  app/channels/gimbal_command_channel.cpp
  app/channels/gimbal_state_channel.cpp
  app/channels/gantry_command_channel.cpp
  app/channels/referee_state_channel.cpp
  app/channels/imu_channel.cpp
  app/protocols/motors/dji_motor_protocol.cpp
  app/protocols/motors/dm_motor_protocol.cpp
  app/protocols/motors/cubemars_motor_protocol.cpp
  app/protocols/mcu_link/mcu_link.cpp
  app/protocols/pc_link/pc_link.cpp
  app/protocols/remote_input/dr16_protocol.cpp
  app/protocols/remote_input/vt03_protocol.cpp
  platform/board/src/board_identity.c
)

if(CONFIG_RM_TEST_RUNTIME_INIT_UART)
  target_sources(app PRIVATE
    platform/drivers/communication/uart_dispatch.cpp
  )
endif()

if(CONFIG_RM_TEST_RUNTIME_INIT_CAN)
  target_sources(app PRIVATE
    platform/drivers/communication/can_dispatch.cpp
  )
endif()

if(CONFIG_RM_TEST_RUNTIME_INIT_USB)
  target_sources(app PRIVATE
    platform/drivers/communication/usb_session.cpp
  )
endif()

target_sources(app PRIVATE
  platform/drivers/devices/actuators/serial_servo.cpp
  platform/drivers/devices/system/referee_client.cpp
)

if(CONFIG_RM_TEST_RUNTIME_INIT_LITTLEFS OR CONFIG_RM_TEST_SHELL_CHASSIS_TUNING)
  target_sources(app PRIVATE
    platform/storage/filesystem/littlefs_service.cpp
  )
endif()

if(CONFIG_RM_TEST_SHELL_CHASSIS_TUNING)
  target_sources(app PRIVATE
    app/debug/shell/chassis_tuning_shell.cpp
  )
endif()

if(CONFIG_SHELL)
  target_sources(app PRIVATE
    app/debug/shell/shell_banner.cpp
  )
endif()
