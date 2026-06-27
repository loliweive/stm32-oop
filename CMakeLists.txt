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

if(BUILD_MODE STREQUAL "stm32f103" OR BUILD_MODE STREQUAL "freertos" OR BUILD_MODE STREQUAL "bootloader" OR BUILD_MODE STREQUAL "peripheral")
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
        "${STM32_SRC_DIR}/app/oled_test"
        "${STM32_SRC_DIR}/display"
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
        ${STM32_SRC_DIR}/utils/assert.c
        ${STM32_SRC_DIR}/hal/gpio.c
        ${STM32_SRC_DIR}/hal/rcc.c
        ${STM32_SRC_DIR}/hal/i2c.c
        ${STM32_SRC_DIR}/display/ssd1306.c
        ${STM32_SRC_DIR}/display/font8x16.c
        ${STM32_SRC_DIR}/display/font6x8.c
        ${STM32_SRC_DIR}/display/font16x16.c
        ${STM32_SRC_DIR}/app/oled_test/main.c
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

    target_link_libraries(${PROJECT_NAME} PRIVATE m)

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

# ── FreeRTOS mode ─────────────────────────────────────────────────
elseif(BUILD_MODE STREQUAL "freertos")
    message(STATUS "=== BUILD MODE: STM32F103C8T6 + FreeRTOS ===")

    # Default: app mode (0x08002000, with bootloader). -DSTANDALONE=ON for direct flash
    if(STANDALONE)
        set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103c8.ld")
    else()
        set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103c8_app.ld")
        message(STATUS "  → App mode (0x08002000, bootloader required)")
    endif()
    set(FREERTOS_DIR   "${CMAKE_SOURCE_DIR}/lib/FreeRTOS")

    set(STM32_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
    set(FREERTOS_INCLUDES
        "${FREERTOS_DIR}/include"
        "${FREERTOS_DIR}/portable/GCC/ARM_CM3"
        "${CMAKE_SOURCE_DIR}/lib/cmsis"
        "${CMAKE_SOURCE_DIR}/lib/stm32f1"
        "${STM32_SRC_DIR}/core"
        "${STM32_SRC_DIR}/hal"
        "${STM32_SRC_DIR}/utils"
        "${STM32_SRC_DIR}/app/freertos_demo"
        "${STM32_SRC_DIR}/app/ds18b20_freertos"
        "${STM32_SRC_DIR}/cli"
        "${STM32_SRC_DIR}/sensor"
        "${STM32_SRC_DIR}/display"
        "${STM32_SRC_DIR}/storage"
        "${STM32_SRC_DIR}/ota"
    )

    # FreeRTOS kernel sources (Cortex-M3/GCC port + heap_4)
    set(FREERTOS_SRC
        ${FREERTOS_DIR}/tasks.c
        ${FREERTOS_DIR}/queue.c
        ${FREERTOS_DIR}/list.c
        ${FREERTOS_DIR}/timers.c
        ${FREERTOS_DIR}/portable/GCC/ARM_CM3/port.c
        ${FREERTOS_DIR}/portable/MemMang/heap_4.c
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
        ${STM32_SRC_DIR}/utils/assert.c
        ${STM32_SRC_DIR}/hal/gpio.c
        ${STM32_SRC_DIR}/hal/uart.c
        ${STM32_SRC_DIR}/hal/rcc.c
        ${STM32_SRC_DIR}/hal/i2c.c
        ${STM32_SRC_DIR}/hal/spi.c
        ${STM32_SRC_DIR}/hal/adc.c
        ${STM32_SRC_DIR}/hal/nvic.c
        ${STM32_SRC_DIR}/hal/timer.c
        ${STM32_SRC_DIR}/sensor/onewire.c
        ${STM32_SRC_DIR}/sensor/ds18b20.c
        ${STM32_SRC_DIR}/sensor/dht11.c
        ${STM32_SRC_DIR}/sensor/light_sensor.c
        ${STM32_SRC_DIR}/sensor/bmp280.c
        ${STM32_SRC_DIR}/hal/iwdg.c
        ${STM32_SRC_DIR}/display/ssd1306.c
        ${STM32_SRC_DIR}/display/font8x16.c
        ${STM32_SRC_DIR}/display/font6x8.c
        ${STM32_SRC_DIR}/display/font16x16.c
        ${STM32_SRC_DIR}/storage/spi_flash.c
        ${STM32_SRC_DIR}/cli/cli.c
        ${STM32_SRC_DIR}/ota/ota_client.c
        ${STM32_SRC_DIR}/ota/ota_flash.c
        ${STM32_SRC_DIR}/ota/ota_protocol.c
        ${STM32_SRC_DIR}/ota/ota_transport_shared.c
        ${STM32_SRC_DIR}/ota/ota_transport_uart.c
        ${STM32_SRC_DIR}/app/ds18b20_freertos/main.c
        ${STM32_SRC_DIR}/app/freertos_demo/freertos_hooks.c
        ${FREERTOS_SRC}
    )

    target_include_directories(${PROJECT_NAME} PRIVATE ${FREERTOS_INCLUDES})

    target_compile_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -O0 -g3 -Wall -Wextra
        -ffunction-sections -fdata-sections -fno-common
        -Wno-maybe-uninitialized
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
        -Wl,--gc-sections
        -T "${LINKER_SCRIPT}"
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE m)

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        STM32F103xB
        USE_FREERTOS
    )

    # Post-build: generate .bin / .hex / size report
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.bin
        COMMAND arm-none-eabi-objcopy -O ihex   $<TARGET_FILE:${PROJECT_NAME}> ${PROJECT_NAME}.hex
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Generating .bin / .hex / size report"
    )

# ── Bootloader mode ────────────────────────────────────────────────
elseif(BUILD_MODE STREQUAL "bootloader")
    message(STATUS "=== BUILD MODE: STM32F103C8T6 OTA Bootloader ===")

    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/linker/stm32f103c8_bootloader.ld")

    set(STM32_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
    set(BOOTLOADER_INCLUDES
        "${CMAKE_SOURCE_DIR}/lib/cmsis"
        "${CMAKE_SOURCE_DIR}/lib/stm32f1"
        "${STM32_SRC_DIR}/core"
        "${STM32_SRC_DIR}/hal"
        "${STM32_SRC_DIR}/utils"
        "${STM32_SRC_DIR}/ota"
        "${STM32_SRC_DIR}/storage"
    )

    add_executable(${PROJECT_NAME}
        ${STM32_SRC_DIR}/startup/startup_stm32f103c8.s
        ${STM32_SRC_DIR}/startup/system_stm32f1.c
        ${STM32_SRC_DIR}/core/ring_buffer.c
        ${STM32_SRC_DIR}/core/list.c
        ${STM32_SRC_DIR}/hal/gpio.c
        ${STM32_SRC_DIR}/hal/rcc.c
        ${STM32_SRC_DIR}/hal/uart.c
        ${STM32_SRC_DIR}/hal/spi.c
        ${STM32_SRC_DIR}/utils/assert.c
        ${STM32_SRC_DIR}/storage/spi_flash.c
        ${STM32_SRC_DIR}/ota/ota_flash.c
        ${STM32_SRC_DIR}/ota/ota_client.c
        ${STM32_SRC_DIR}/ota/ota_protocol.c
        ${STM32_SRC_DIR}/ota/ota_transport_uart.c
        ${STM32_SRC_DIR}/ota/ota_transport_shared.c
        ${STM32_SRC_DIR}/bootloader/main.c
    )

    target_include_directories(${PROJECT_NAME} PRIVATE ${BOOTLOADER_INCLUDES})

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
        "${CMAKE_SOURCE_DIR}/lib/cmsis"
        "${CMAKE_SOURCE_DIR}/lib/stm32f1"
        "${STM32_SRC_DIR}/core"
        "${STM32_SRC_DIR}/hal"
        "${STM32_SRC_DIR}/utils"
        "${STM32_SRC_DIR}/display"
    )

    # Common sources for all peripheral tests
    set(PERIPHERAL_COMMON_SRC
        ${STM32_SRC_DIR}/startup/startup_stm32f103c8.s
        ${STM32_SRC_DIR}/startup/system_stm32f1.c
        ${STM32_SRC_DIR}/hal/gpio.c
        ${STM32_SRC_DIR}/hal/rcc.c
    )

    # Per-peripheral extra sources (declared before add_executable)
    set(PERIPHERAL_EXTRA_SRC "")

    if(PERIPHERAL STREQUAL "led")
        # LED only needs gpio+rcc — no extra sources
    elseif(PERIPHERAL STREQUAL "uart_echo")
        list(APPEND PERIPHERAL_EXTRA_SRC
            ${STM32_SRC_DIR}/hal/rcc.c
        )
    elseif(PERIPHERAL STREQUAL "oled")
        list(APPEND PERIPHERAL_EXTRA_SRC
            ${STM32_SRC_DIR}/hal/i2c.c
            ${STM32_SRC_DIR}/display/ssd1306.c
            ${STM32_SRC_DIR}/display/font8x16.c
            ${STM32_SRC_DIR}/display/font6x8.c
            ${STM32_SRC_DIR}/display/font16x16.c
            ${STM32_SRC_DIR}/utils/delay.c
        )
    elseif(PERIPHERAL STREQUAL "button")
        # GPIO only
    elseif(PERIPHERAL STREQUAL "spi_flash")
        list(APPEND PERIPHERAL_EXTRA_SRC
            ${STM32_SRC_DIR}/hal/spi.c
        )
    elseif(PERIPHERAL STREQUAL "ds18b20")
        # Pure GPIO bit-bang — no extra sources
    else()
        message(FATAL_ERROR "Unknown PERIPHERAL: '${PERIPHERAL}'. Valid: led, uart_echo, oled, button, spi_flash, ds18b20")
    endif()

    add_executable(${PROJECT_NAME}
        ${PERIPHERAL_COMMON_SRC}
        ${PERIPHERAL_EXTRA_SRC}
        ${TEST_SRC_DIR}/test_${PERIPHERAL}.c
    )

    target_include_directories(${PROJECT_NAME} PRIVATE ${PERIPHERAL_INCLUDES})

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

    target_link_libraries(${PROJECT_NAME} PRIVATE m)

    target_compile_definitions(${PROJECT_NAME} PRIVATE STM32F103xB)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}>
                "${TEST_SRC_DIR}/bin/test_${PERIPHERAL}.bin"
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Saved: tests/peripheral/bin/test_${PERIPHERAL}.bin"
    )

else()
    message(FATAL_ERROR "Unknown BUILD_MODE: '${BUILD_MODE}'. Use 'host', 'stm32f103', 'freertos', 'bootloader', or 'peripheral'.")
endif()
