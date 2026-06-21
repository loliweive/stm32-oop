cmake_minimum_required(VERSION 3.20)

# ── Dual-mode build ──────────────────────────────────────────────
# BUILD_MODE=host      : x86 host tests (default)
# BUILD_MODE=stm32f103 : cross-compile for STM32F103C8T6
#
# Toolchain file MUST be set before project().
# ─────────────────────────────────────────────────────────────────

if(NOT DEFINED BUILD_MODE)
    set(BUILD_MODE host)
endif()

if(BUILD_MODE STREQUAL "stm32f103")
    set(CMAKE_TOOLCHAIN_FILE
        "${CMAKE_CURRENT_LIST_DIR}/cmake/toolchain-arm-none-eabi.cmake"
        CACHE STRING "" FORCE)
endif()

project(stm32-oop C ASM)

# ── Host mode ────────────────────────────────────────────────────
if(BUILD_MODE STREQUAL "host")
    message(STATUS "=== BUILD MODE: HOST (unit tests) ===")
    enable_testing()
    add_subdirectory(test)

# ── Target mode ──────────────────────────────────────────────────
elseif(BUILD_MODE STREQUAL "stm32f103")
    message(STATUS "=== BUILD MODE: STM32F103C8T6 (cross-compile) ===")

    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103c8.ld")

    set(STM32_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
    set(STM32_INCLUDES
        "${CMAKE_SOURCE_DIR}/lib/cmsis"
        "${CMAKE_SOURCE_DIR}/lib/stm32f1"
        "${STM32_SRC_DIR}/core"
        "${STM32_SRC_DIR}/hal"
        "${STM32_SRC_DIR}/utils"
        "${STM32_SRC_DIR}/app/blinky"
    )

    add_executable(${PROJECT_NAME}
        ${STM32_SRC_DIR}/startup/startup_stm32f103c8.s
        ${STM32_SRC_DIR}/startup/system_stm32f1.c
        ${STM32_SRC_DIR}/core/object.c
        ${STM32_SRC_DIR}/core/ring_buffer.c
        ${STM32_SRC_DIR}/core/list.c
        ${STM32_SRC_DIR}/core/state_machine.c
        ${STM32_SRC_DIR}/utils/log.c
        ${STM32_SRC_DIR}/utils/delay.c
        ${STM32_SRC_DIR}/hal/gpio.c
        ${STM32_SRC_DIR}/hal/uart.c
        ${STM32_SRC_DIR}/hal/rcc.c
        ${STM32_SRC_DIR}/hal/timer.c
        ${STM32_SRC_DIR}/hal/i2c.c
        ${STM32_SRC_DIR}/hal/spi.c
        ${STM32_SRC_DIR}/hal/adc.c
        ${STM32_SRC_DIR}/hal/nvic.c
        ${STM32_SRC_DIR}/app/blinky/main.c
    )

    target_include_directories(${PROJECT_NAME} PRIVATE ${STM32_INCLUDES})

    target_compile_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -Os -g3 -Wall -Wextra
        -ffunction-sections -fdata-sections -fno-common
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -Wl,--gc-sections
        -T "${LINKER_SCRIPT}"
    )

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        STM32F103xB
    )

    # Post-build: generate .bin / .hex / size report
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
        COMMAND arm-none-eabi-objcopy -O ihex   $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Generating .bin / .hex / size report"
    )

else()
    message(FATAL_ERROR "Unknown BUILD_MODE: '${BUILD_MODE}'. Use 'host' or 'stm32f103'.")
endif()
