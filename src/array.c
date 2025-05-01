#include "array.h"

bool mongory_array_each(mongory_array *self, void *acc, mongory_iterable_callback_func func) {
  return mongory_iterable_each(&self->base, acc, func);
}