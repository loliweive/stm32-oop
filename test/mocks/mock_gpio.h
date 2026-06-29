/**
 * Mock GPIO for host-side testing.
 * Includes the real gpio.h interface but provides mock implementations
 * that record calls instead of touching hardware registers.
 */

#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>
#include <stdbool.h>

/* Reuse the real type definitions */
#include "gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool     initialized;
    uint8_t  level;
    uint8_t  mode;
    uint16_t pin;
    int      set_count;
    int      toggle_count;
} MockGpioState;

/** Reset mock state for a new test. */
void mock_gpio_reset(MockGpioState *s);

/** Inject a mock vtable into a GpioPin (replaces the real one). */
void mock_gpio_inject(GpioPin *pin, MockGpioState *state);

/** Get the mock vtable pointer (for use by mock constructors). */
const GpioVtable *mock_gpio_get_vtable(void);

/** Set global mock state (before spi_flash_init in link-time mock setup). */
void mock_gpio_set_global(MockGpioState *s);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_GPIO_H */
