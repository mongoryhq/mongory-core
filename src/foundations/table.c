#include "array_private.h"
#include <mongory-core/foundations/array.h>
#include <mongory-core/foundations/config.h>
#include <mongory-core/foundations/table.h>
#include <mongory-core/foundations/value.h>
#include <string.h>

#define MONGORY_TABLE_INIT_SIZE 17
#define MONGORY_TABLE_LOAD_FACTOR 0.75

typedef struct mongory_table_internal {
  mongory_table base;   // public table structure
  size_t capacity;      // current capacity of the table
  mongory_array *array; // base array to hold table nodes
} mongory_table_internal;

typedef struct mongory_table_node {
  char *key;
  mongory_value *value;
  struct mongory_table_node *next;
} mongory_table_node;

mongory_table_node *mongory_table_node_new(mongory_table *self) {
  return self->pool->alloc(self->pool->ctx, sizeof(mongory_table_node));
}

static inline size_t next_prime(size_t n) {
  if (n <= 2) {
    return 2;
  }
  if (n % 2 == 0)
    n++;

  while (1) {
    bool is_prime = true;
    for (size_t i = 3; i * i <= n; i += 2) {
      if (n % i == 0) {
        is_prime = false;
        break;
      }
    }
    if (is_prime) {
      return n;
    }
    n += 2;
  }
}

static inline size_t hash_string(const char *str) {
  size_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

static inline bool
mongory_table_node_walk(mongory_table_node *head, void *acc,
                        bool (*callback)(mongory_table_node *node, void *acc)) {
  for (mongory_table_node *node = head; node;) {
    mongory_table_node *next = node->next;
    if (!callback(node, acc)) {
      return false;
    }
    node = next;
  }

  return true;
}

static inline bool mongory_table_rehash_on_node(mongory_table_node *node,
                                                void *acc) {
  mongory_table_internal *internal = (mongory_table_internal *)acc;
  mongory_array *array = internal->array;
  size_t new_index = hash_string(node->key) % internal->capacity;
  mongory_table_node *root_node =
      (mongory_table_node *)array->get(array, new_index);
  node->next = root_node;
  array->set(array, new_index, (mongory_value *)node);
  return true;
}

static inline bool mongory_table_rehash(mongory_table *self) {
  mongory_table_internal *internal = (mongory_table_internal *)self;
  mongory_array_private *array = (mongory_array_private *)internal->array;
  mongory_value **origin_items = array->items;
  size_t origin_size =
      internal->capacity; // This is also old iter->capacity and iter->count
  size_t new_size = next_prime(origin_size * 2); // New table capacity
  array->base.count = 0; // Ignore reassign elements in array resize

  if (!mongory_array_resize(internal->array, new_size)) {
    return false;
  }

  internal->capacity = new_size;             // Update the table capacity
  for (size_t i = 0; i < origin_size; i++) { // Iterate up to old capacity
    mongory_table_node_walk((mongory_table_node *)origin_items[i], self,
                            mongory_table_rehash_on_node);
  }

  return true;
}

typedef struct mongory_table_kv_context {
  char *key;
  mongory_value *value;
} mongory_table_kv_context;

static inline bool mongory_table_get_on_node(mongory_table_node *node,
                                             void *acc) {
  mongory_table_kv_context *ctx = (mongory_table_kv_context *)acc;
  if (strcmp(node->key, ctx->key) == 0) {
    ctx->value = node->value;
    return false;
  }

  return true;
}

mongory_value *mongory_table_get(mongory_table *self, char *key) {
  mongory_table_internal *internal = (mongory_table_internal *)self;
  mongory_array *array = internal->array;
  size_t index = hash_string(key) % internal->capacity;
  mongory_table_node *root_node =
      (mongory_table_node *)array->get(array, index);
  mongory_table_kv_context ctx = {key, NULL};
  mongory_table_node_walk(root_node, &ctx, mongory_table_get_on_node);
  return ctx.value;
}

static inline bool mongory_table_set_on_node(mongory_table_node *node,
                                             void *acc) {
  mongory_table_kv_context *ctx = (mongory_table_kv_context *)acc;
  if (strcmp(node->key, ctx->key) == 0) {
    node->value = ctx->value;
    return false;
  }

  return true;
}

bool mongory_table_set(mongory_table *self, char *key, mongory_value *value) {
  mongory_table_internal *internal = (mongory_table_internal *)self;
  mongory_array *array = internal->array;
  size_t index = hash_string(key) % internal->capacity;
  mongory_table_node *root_node =
      (mongory_table_node *)array->get(array, index);
  mongory_table_kv_context ctx = {key, value};
  if (!mongory_table_node_walk(root_node, &ctx, mongory_table_set_on_node)) {
    return true;
  }

  mongory_table_node *new_node = mongory_table_node_new(self);
  if (!new_node) {
    return false;
  }

  key = mongory_string_cpy(self->pool, key);
  if (!key) {
    return false;
  }

  new_node->key = key;
  new_node->value = value;
  new_node->next = root_node;
  array->set(array, index, (mongory_value *)new_node);

  self->count++;
  if (self->count > internal->capacity * MONGORY_TABLE_LOAD_FACTOR) {
    mongory_table_rehash(self);
  }
  return true;
}

bool mongory_table_del(mongory_table *self, char *key) {
  mongory_table_internal *internal = (mongory_table_internal *)self;
  mongory_array *array = internal->array;
  size_t index = hash_string(key) % internal->capacity;
  mongory_table_node *node = (mongory_table_node *)array->get(array, index);
  mongory_table_node *prev = NULL;
  while (node) {
    if (strcmp(node->key, key) == 0) {
      if (prev) {
        prev->next = node->next;
      } else {
        array->set(array, index, (mongory_value *)node->next);
      }
      self->count--;
      return true;
    }

    prev = node;
    node = node->next;
  }

  return false;
}

typedef struct mongory_table_each_pair_context {
  void *acc;
  mongory_table_each_pair_callback_func callback;
} mongory_table_each_pair_context;

static inline bool mongory_table_each_pair_on_node(mongory_table_node *node,
                                                   void *acc) {
  mongory_table_each_pair_context *ctx = (mongory_table_each_pair_context *)acc;
  return ctx->callback(node->key, node->value, ctx->acc);
}

static inline bool mongory_table_each_pair_on_root(mongory_value *value,
                                                   void *acc) {
  mongory_table_node *node = (mongory_table_node *)value;
  return mongory_table_node_walk(node, acc, mongory_table_each_pair_on_node);
}

bool mongory_table_each_pair(mongory_table *self, void *acc,
                             mongory_table_each_pair_callback_func callback) {
  mongory_table_internal *internal = (mongory_table_internal *)self;
  mongory_table_each_pair_context each_ctx = {acc, callback};
  return internal->array->each(internal->array, &each_ctx,
                               mongory_table_each_pair_on_root);
}

mongory_table *mongory_table_new(mongory_memory_pool *pool) {
  size_t init_size = MONGORY_TABLE_INIT_SIZE;
  mongory_array *array = mongory_array_new(pool);
  bool array_init_success = array && mongory_array_resize(array, init_size) &&
                            array->set(array, init_size - 1, NULL);
  if (!array_init_success) {
    return NULL;
  }

  mongory_table_internal *internal =
      pool->alloc(pool->ctx, sizeof(mongory_table_internal));
  if (!internal) {
    return NULL;
  }

  internal->base.pool = pool;
  internal->array = array;
  internal->capacity = init_size;
  internal->base.count = 0;

  internal->base.each = mongory_table_each_pair;
  internal->base.get = mongory_table_get;
  internal->base.set = mongory_table_set;
  internal->base.del = mongory_table_del;

  return &internal->base;
}
