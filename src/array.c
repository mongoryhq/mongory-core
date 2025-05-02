#include "private.h"
#include "array.h"

struct mongory_array {
  mongory_iterable base;
};

bool mongory_array_each(mongory_array *self, void *acc, mongory_array_callback_func func) {
  return mongory_iterable_each(&self->base, acc, func);
}