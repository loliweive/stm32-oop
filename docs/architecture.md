# STM32-oop Architecture

## Overview

A reusable, object-oriented C micro-framework for STM32F103C8T6 (Cortex-M3, 64KB Flash, 20KB SRAM). Designed for learning embedded OOP patterns and TDD on bare-metal.

## Build System

Dual-mode CMake:

| Mode | Command | Output |
|------|---------|--------|
| Host tests | `cmake -B build/test -DBUILD_MODE=host` | x86 executables, `ctest` |
| Target firmware | `cmake -B build/target -DBUILD_MODE=stm32f103` | `stm32-oop.elf/.bin/.hex` |

The toolchain file (`cmake/toolchain-arm-none-eabi.cmake`) must be set before `project()` — the root `CMakeLists.txt` handles this based on `BUILD_MODE`.

## Layers

```
app/          User experiments (blinky, uart_echo, breathing)
hal/          OOP peripheral wrappers (GPIO, UART, I2C, SPI, Timer, ADC, RCC)
core/         Runtime: Object, RingBuffer, List, StateMachine
utils/        Assert, Log, Delay
lib/          CMSIS + STM32F103 register definitions
```

## OOP Pattern: vtable Dispatch

Each HAL "class" uses a struct with a const vtable pointer:

```c
struct GpioPin {
    void *port; uint16_t pin; uint8_t mode;
    const GpioVtable *vtable;  // ← inject mock vtable for testing
};
static inline void gpio_set(GpioPin *self, uint8_t level) {
    self->vtable->set(self, level);  // dispatches to real or mock
}
```

## Dependency Injection for Testing

UART demonstrates injection: the `io` field is a `SerialInterface*` that can be a real USART or a mock. When `io != NULL`, all I/O goes through the injected interface — no hardware access.

## Test Strategy

| Layer | Where | Framework | Approach |
|-------|-------|-----------|----------|
| core (ring_buffer, list, state_machine) | Host | Unity | Pure logic, no mocks needed |
| hal (GPIO, UART, Timer) | Host | Unity + mocks | Mock vtable/injected IO |
| hardware | Target | Manual | LED, UART echo, scope |

## Memory Budget

- **Flash**: 608 bytes current (64KB available) — 0.9% used
- **SRAM**: 4 bytes bss + stack (20KB available)
- **No dynamic allocation**: All buffers statically allocated

## Adding a New Peripheral

1. Define register struct and macros in `lib/stm32f1/stm32f103xb.h`
2. Create `src/hal/<periph>.h` with vtable + inline dispatchers
3. Create `src/hal/<periph>.c` with real implementation
4. Create `test/mocks/mock_<periph>.h` and `.c`
5. Create `test/test_<periph>.c`
6. Add to `test/CMakeLists.txt`
7. Optionally add to target `CMakeLists.txt` if the experiment needs it

## FreeRTOS Integration Path

When ready: replace the cooperative main-loop scheduler with `xTaskCreate`. The HAL layer is already thread-safe-ish (UART uses ring buffers). Add mutex guards for shared peripherals (I2C, SPI).
