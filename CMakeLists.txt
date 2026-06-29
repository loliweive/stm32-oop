cmake_minimum_required(VERSION 3.20)

# ── Bare-metal build (no RTOS) ────────────────────────────────────
# BUILD_MODE=host       : x86 host tests (default)
# BUILD_MODE=stm32f103  : bare-metal CLI + sensors
# BUILD_MODE=peripheral : single-peripheral test firmware
# ─────────────────────────────────────────────────────────────────

if(NOT DEFINED BUILD_MODE)
    set(BUILD_MODE host)
endif()

if(BUILD_MODE STREQUAL "stm32f103" OR BUILD_MODE STREQUAL "peripheral")
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

# ── Bare-metal target mode ───────────────────────────────────────
elseif(BUILD_MODE STREQUAL "stm32f103")
    message(STATUS "=== BUILD MODE: STM32F103C8T6 Bare-Metal ===")

    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103c8.ld")
    set(STM32_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
    set(STM32_INCLUDES
        "${CMAKE_SOURCE_DIR}/lib/cmsis"
        "${CMAKE_SOURCE_DIR}/lib/stm32f1"
        "${STM32_SRC_DIR}/core"
        "${STM32_SRC_DIR}/hal"
        "${STM32_SRC_DIR}/utils"
        "${STM32_SRC_DIR}/cli"
        "${STM32_SRC_DIR}/sensor"
        "${STM32_SRC_DIR}/display"
        "${STM32_SRC_DIR}/storage"
        "${STM32_SRC_DIR}/app/bare_cli"
    )

    add_executable(${PROJECT_NAME}
        ${STM32_SRC_DIR}/startup/startup_stm32f103c8.s
        ${STM32_SRC_DIR}/startup/system_stm32f1.c
        ${STM32_SRC_DIR}/core/ring_buffer.c
        ${STM32_SRC_DIR}/core/list.c
        ${STM32_SRC_DIR}/core/state_machine.c
        ${STM32_SRC_DIR}/utils/log.c
        ${STM32_SRC_DIR}/utils/delay.c
        ${STM32_SRC_DIR}/utils/assert.c
        ${STM32_SRC_DIR}/utils/crc32.c
        ${STM32_SRC_DIR}/hal/gpio.c
        ${STM32_SRC_DIR}/hal/rcc.c
        ${STM32_SRC_DIR}/hal/i2c.c
        ${STM32_SRC_DIR}/hal/uart.c
        ${STM32_SRC_DIR}/hal/spi.c
        ${STM32_SRC_DIR}/hal/adc.c
        ${STM32_SRC_DIR}/hal/iwdg.c
        ${STM32_SRC_DIR}/sensor/dht11.c
        ${STM32_SRC_DIR}/sensor/light_sensor.c
        ${STM32_SRC_DIR}/display/ssd1306.c
        ${STM32_SRC_DIR}/display/font8x16.c
        ${STM32_SRC_DIR}/display/font6x8.c
        ${STM32_SRC_DIR}/display/font16x16.c
        ${STM32_SRC_DIR}/storage/spi_flash.c
        ${STM32_SRC_DIR}/cli/cli.c
        ${STM32_SRC_DIR}/app/bare_cli/main.c
    )

    target_include_directories(${PROJECT_NAME} PRIVATE ${STM32_INCLUDES})
    target_compile_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -Os -g3 -Wall -Wextra -Werror
        -ffunction-sections -fdata-sections -fno-common
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -Wl,--gc-sections -T "${LINKER_SCRIPT}"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
    target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F103xB)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
        COMMAND arm-none-eabi-objcopy -O ihex   $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Generating .bin / .hex / size report"
    )

# ── Peripheral test mode ──────────────────────────────────────────
elseif(BUILD_MODE STREQUAL "peripheral")
    if(NOT DEFINED PERIPHERAL)
        message(FATAL_ERROR "PERIPHERAL is required. Usage: cmake -B build -DBUILD_MODE=peripheral -DPERIPHERAL=led")
    endif()
    message(STATUS "=== BUILD MODE: STM32F103C8T6 Peripheral Test: ${PERIPHERAL} ===")

    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103c8.ld")
    set(STM32_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
    set(TEST_SRC_DIR  "${CMAKE_SOURCE_DIR}/tests/peripheral")
    set(PERIPHERAL_INCLUDES
        "${CMAKE_SOURCE_DIR}/lib/cmsis" "${CMAKE_SOURCE_DIR}/lib/stm32f1"
        "${STM32_SRC_DIR}/core" "${STM32_SRC_DIR}/hal" "${STM32_SRC_DIR}/utils" "${STM32_SRC_DIR}/display"
    )
    set(PERIPHERAL_COMMON_SRC
        ${STM32_SRC_DIR}/startup/startup_stm32f103c8.s
        ${STM32_SRC_DIR}/startup/system_stm32f1.c
        ${STM32_SRC_DIR}/hal/gpio.c ${STM32_SRC_DIR}/hal/rcc.c
    )
    set(PERIPHERAL_EXTRA_SRC "")
    if(PERIPHERAL STREQUAL "led")
    elseif(PERIPHERAL STREQUAL "uart_echo")
        list(APPEND PERIPHERAL_EXTRA_SRC ${STM32_SRC_DIR}/hal/rcc.c)
    elseif(PERIPHERAL STREQUAL "oled")
        list(APPEND PERIPHERAL_EXTRA_SRC ${STM32_SRC_DIR}/hal/i2c.c ${STM32_SRC_DIR}/display/font8x16.c ${STM32_SRC_DIR}/display/font6x8.c ${STM32_SRC_DIR}/display/font16x16.c ${STM32_SRC_DIR}/display/ssd1306.c)
    elseif(PERIPHERAL STREQUAL "button")
    elseif(PERIPHERAL STREQUAL "spi_flash")
        list(APPEND PERIPHERAL_EXTRA_SRC ${STM32_SRC_DIR}/hal/spi.c ${STM32_SRC_DIR}/storage/spi_flash.c)
    elseif(PERIPHERAL STREQUAL "ds18b20")
        list(APPEND PERIPHERAL_EXTRA_SRC ${STM32_SRC_DIR}/sensor/onewire.c ${STM32_SRC_DIR}/sensor/ds18b20.c)
    else()
        message(FATAL_ERROR "Unknown PERIPHERAL: '${PERIPHERAL}'. Valid: led, uart_echo, oled, button, spi_flash, ds18b20")
    endif()
    add_executable(${PROJECT_NAME}
        ${PERIPHERAL_COMMON_SRC} ${PERIPHERAL_EXTRA_SRC}
        "${TEST_SRC_DIR}/test_${PERIPHERAL}.c"
    )
    target_include_directories(${PROJECT_NAME} PRIVATE ${PERIPHERAL_INCLUDES})
    target_compile_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -Os -g3 -Wall -Wextra -ffunction-sections -fdata-sections -fno-common
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft -Wl,--gc-sections -T "${LINKER_SCRIPT}"
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE m)
    target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F103xB)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}> "${TEST_SRC_DIR}/bin/test_${PERIPHERAL}.bin"
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Saved: tests/peripheral/bin/test_${PERIPHERAL}.bin"
    )
else()
    message(FATAL_ERROR "Unknown BUILD_MODE: '${BUILD_MODE}'. Use 'host', 'stm32f103', or 'peripheral'.")
endif()
