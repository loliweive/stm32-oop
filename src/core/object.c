#include "object.h"

void object_ctor(Object *self, const char *type_name)
{
    self->type_name = type_name;
    self->destroy   = object_destroy;
}

void object_destroy(Object *self)
{
    /* Base does nothing — subclasses override via vtable. */
    (void)self;
}
