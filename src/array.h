#ifndef MONGORY_ARRAY
#define MONGORY_ARRAY
#include <stdbool.h>

struct mongory_array;
typedef struct mongory_array mongory_array;

#include "private.h"
bool mongory_array_each(mongory_array *self, void *acc, mongory_iterable_callback_func func);

struct mongory_array {
  mongory_iterable base;
};

#endif