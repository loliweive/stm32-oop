/** Mock GPIO for host-side TDD. Records all calls for verification. */
#include "mock_gpio.h"
#include <string.h>

static MockGpioState *active_state = NULL;

static void _mock_init(GpioPin *self)
{
    (void)self;
    if (active_state) active_state->initialized = true;
}

static void _mock_set(GpioPin *self, uint8_t level)
{
    (void)self;
    if (active_state) {
        active_state->level = level;
        active_state->set_count++;
    }
}

static uint8_t _mock_get(GpioPin *self)
{
    (void)self;
    return active_state ? active_state->level : 0;
}

static void _mock_toggle(GpioPin *self)
{
    (void)self;
    if (active_state) {
        active_state->level = !active_state->level;
        active_state->toggle_count++;
    }
}

static void _mock_set_mode(GpioPin *self, uint8_t mode)
{
    (void)self;
    if (active_state) active_state->mode = mode;
}

static const GpioVtable _mock_vtable = {
    .init     = _mock_init,
    .set      = _mock_set,
    .get      = _mock_get,
    .toggle   = _mock_toggle,
    .set_mode = _mock_set_mode,
};

void mock_gpio_reset(MockGpioState *s)
{
    memset(s, 0, sizeof(*s));
}

void mock_gpio_inject(GpioPin *pin, MockGpioState *state)
{
    pin->vtable = &_mock_vtable;
    /* Update the state pointer to this mock's state.
       In a real multi-instance scenario you'd use a per-pin context,
       but for single-pin tests this global is sufficient. */
    active_state = state;
    state->pin   = pin->pin;
}
