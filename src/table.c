#include "table.h"

bool mongory_table_each(mongory_table *self, void *acc, mongory_table_callback_func func) {
  mongory_table_each_context ctx = {
    .acc = acc,
    .callback = func
  };
  return mongory_iterable_each(&self->base, &ctx, mongory_table_traverse_node_chain);
}