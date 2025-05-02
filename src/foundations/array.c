#include <mongory-core/foundations/iterable.h>
#include <mongory-core/foundations/array.h>

bool mongory_array_each(mongory_array *self, void *acc, mongory_array_callback_func func) {
  return mongory_iterable_each((mongory_iterable *)self->base, acc, func);
}