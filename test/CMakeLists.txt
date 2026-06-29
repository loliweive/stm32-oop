# Host-mode unit test build — runs on x86.
# See test_*.c files for individual test suites.
cmake_minimum_required(VERSION 3.20)

set(TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ROOT_DIR  "${CMAKE_SOURCE_DIR}")

# ── Unity framework ──────────────────────────────────────────
set(UNITY_SRC
    ${TEST_DIR}/unity/unity.c
)

# ── Source under test (host-compilable modules only) ─────────
set(CORE_SRC
    ${ROOT_DIR}/src/core/object.c
    ${ROOT_DIR}/src/core/ring_buffer.c
    ${ROOT_DIR}/src/core/list.c
    ${ROOT_DIR}/src/core/state_machine.c
)

set(UTILS_SRC
    ${ROOT_DIR}/src/utils/assert.c
)

# ── Test executables ─────────────────────────────────────────
set(TEST_INCLUDES
    ${TEST_DIR}
    ${TEST_DIR}/unity
    ${TEST_DIR}/mocks
    ${ROOT_DIR}/src/core
    ${ROOT_DIR}/src/utils
    ${ROOT_DIR}/src/hal
    ${ROOT_DIR}/src/cli
    ${ROOT_DIR}/src/ota
    ${ROOT_DIR}/src/display
    ${ROOT_DIR}/lib/stm32f1
    ${ROOT_DIR}/lib/cmsis
)

# Mock header overrides — for host, we provide mock implementations
# of the hardware headers so that HAL code compiles on x86.
# test/mocks/ provides replacement headers for target-specific includes.

function(add_host_test name)
    add_executable(${name}
        ${ARGN}
        ${UNITY_SRC}
    )
    target_include_directories(${name} PRIVATE ${TEST_INCLUDES})
    target_compile_options(${name} PRIVATE -Wall -Wextra -g -O0)
    target_compile_definitions(${name} PRIVATE HOST_TEST)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── Tests ────────────────────────────────────────────────────
add_host_test(test_ring_buffer
    ${CORE_SRC}
    ${TEST_DIR}/test_ring_buffer.c
)

add_host_test(test_list
    ${CORE_SRC}
    ${TEST_DIR}/test_list.c
)

add_host_test(test_state_machine
    ${CORE_SRC}
    ${TEST_DIR}/test_state_machine.c
)

add_host_test(test_assert
    ${CORE_SRC}
    ${UTILS_SRC}
    ${TEST_DIR}/test_assert.c
)

# Mock implementations — provide fake hardware for HAL tests
set(MOCK_SRC
    ${TEST_DIR}/mocks/mock_gpio.c
    ${TEST_DIR}/mocks/mock_uart.c
)

# HAL source under test (rcc.c needed by uart_init → rcc_get_config)
set(HAL_SRC
    ${ROOT_DIR}/src/hal/gpio.c
    ${ROOT_DIR}/src/hal/uart.c
    ${ROOT_DIR}/src/hal/rcc.c
)

add_host_test(test_gpio
    ${CORE_SRC}
    ${HAL_SRC}
    ${MOCK_SRC}
    ${TEST_DIR}/test_gpio.c
)

add_host_test(test_uart
    ${CORE_SRC}
    ${HAL_SRC}
    ${MOCK_SRC}
    ${TEST_DIR}/test_uart.c
)

# Timer mock + test
set(TIMER_SRC
    ${ROOT_DIR}/src/hal/timer.c
)
set(MOCK_TIMER_SRC
    ${TEST_DIR}/mocks/mock_timer.c
)

add_host_test(test_timer
    ${CORE_SRC}
    ${TIMER_SRC}
    ${MOCK_TIMER_SRC}
    ${TEST_DIR}/test_timer.c
)

# OTA protocol test (pure logic, no hardware needed)
set(OTA_SRC
    ${ROOT_DIR}/src/ota/ota_protocol.c
)

add_host_test(test_ota_protocol
    ${OTA_SRC}
    ${TEST_DIR}/test_ota_protocol.c
)

# SSD1306 OLED test (framebuffer operations, no I2C needed)
set(SSD1306_SRC
    ${ROOT_DIR}/src/display/ssd1306.c
    ${ROOT_DIR}/src/display/font8x16.c
    ${ROOT_DIR}/src/display/font6x8.c
    ${ROOT_DIR}/src/display/font16x16.c
)

add_host_test(test_ssd1306
    ${SSD1306_SRC}
    ${TEST_DIR}/test_ssd1306.c
)

# CRC32 test (pure logic, no hardware needed)
set(CRC32_SRC
    ${ROOT_DIR}/src/utils/crc32.c
)

add_host_test(test_crc32
    ${CRC32_SRC}
    ${TEST_DIR}/test_crc32.c
)

# CLI test (pure logic, mock write_fn)
set(CLI_SRC
    ${ROOT_DIR}/src/cli/cli.c
)

add_host_test(test_cli
    ${CLI_SRC}
    ${TEST_DIR}/test_cli.c
)
