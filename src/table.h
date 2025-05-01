#ifndef MONGORY_TABLE
#define MONGORY_TABLE
#include <stdbool.h>

struct mongory_table;
typedef struct mongory_table mongory_table;

#include "private.h"
#include "value.h"
typedef bool (mongory_table_callback_func)(char *key, mongory_value *value, void *acc);
bool mongory_table_each(mongory_table *self, void *acc, mongory_table_callback_func func);

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

static inline bool mongory_table_traverse_node_chain(void *node_ptr, void *ctx_ptr) {
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

#endif
