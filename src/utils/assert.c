#include "assert.h"
#include <stdio.h>
#include <stdlib.h>

static AssertHandler current_handler = NULL;

void assert_set_handler(AssertHandler handler)
{
    current_handler = handler;
}

void _assert_fail(const char *file, int line, const char *msg)
{
    if (current_handler) {
        current_handler(file, line, msg);
    } else {
        /* Default: print and abort */
        fprintf(stderr, "ASSERT FAILED: %s:%d — %s\n", file, line, msg);
        abort();
    }
}
