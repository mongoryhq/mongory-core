#include "private.h"
#include "value.h"
#include "table.h"

struct mongory_table {
  mongory_iterable base;
};

typedef struct mongory_table_node {
  char *key;
  mongory_value *value;
  struct mongory_table_node *next;
} mongory_table_node;

typedef struct mongory_table_each_context {
  mongory_table_callback_func *callback;
  void *acc;
} mongory_table_each_context;

inline bool mongory_table_traverse_node_chain(void *node_ptr, void *ctx_ptr) {
  mongory_table_node *node = (mongory_table_node *)node_ptr;
  mongory_table_each_context *ctx = (mongory_table_each_context *)ctx_ptr;
  while (node) {
    if (!ctx->callback(node->key, node->value, ctx->acc)) {
      return false;
    }
    node = node->next;
  }
  return true;
}

bool mongory_table_each(mongory_table *self, void *acc, mongory_table_callback_func func) {
  mongory_table_each_context ctx = {
    .acc = acc,
    .callback = func
  };
  return mongory_iterable_each(&self->base, &ctx, mongory_table_traverse_node_chain);
}