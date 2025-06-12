#include <string.h>
#include "iterable.h"
#include <mongory-core/foundations/value.h>
#include <mongory-core/foundations/table.h>

#define MONGORY_TABLE_INIT_SIZE 17
#define MONGORY_TABLE_LOAD_FACTOR 0.75

typedef struct mongory_table_node {
  char *key;
  mongory_value *value;
  struct mongory_table_node *next;
} mongory_table_node;

mongory_table_node* mongory_table_node_new(mongory_table *self) {
  return self->pool->alloc(self->pool->ctx, sizeof(mongory_table_node));
}

static size_t next_prime(size_t n) {
  if (n <= 2) return 2;
  if (n % 2 == 0) n++;

  while (1) {
    bool is_prime = true;
    for (size_t i = 3; i * i <= n; i += 2) {
      if (n % i == 0) {
        is_prime = false;
        break;
      }
    }
    if (is_prime) return n;
    n += 2;
  }
}

static size_t hash_string(const char *str) {
  size_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash;
}

static bool mongory_table_node_walk(mongory_table_node *head, void *acc, bool (*callback)(mongory_table_node *node, void *acc)) {
  for (mongory_table_node *node = head; node;) {
    mongory_table_node *next = node->next;
    if (!callback(node, acc)) return false;
    node = next;
  }

  return true;
}

static bool mongory_table_rehash_on_node(mongory_table_node *node, void *acc) {
  mongory_table *self = (mongory_table *)acc;
  mongory_iterable *iter = (mongory_iterable *)self->base;
  size_t new_index = hash_string(node->key) % self->capacity;
  node->next = iter->items[new_index];
  iter->items[new_index] = node;
  return true;
}

static bool mongory_table_rehash(mongory_table *self) {
  mongory_iterable *iter = (mongory_iterable *)self->base;
  size_t origin_size = self->capacity; // This is also old iter->capacity and iter->count
  void **origin_items = iter->items; // Capture old items array

  self->capacity = next_prime(origin_size * 2); // New table capacity

  // Resize iterable to the new table capacity
  if (!mongory_iterable_resize(iter, self->capacity)) {
    // Failed to resize, table capacity should revert, or handle error more robustly
    self->capacity = origin_size; // Revert capacity
    return false;
  }

  // Initialize all slots in the new iterable items array to NULL before rehashing
  for (size_t i = 0; i < self->capacity; ++i) {
    iter->items[i] = NULL;
  }
  // After resize, iter->capacity is self->capacity. Update iter->count as well.
  iter->count = self->capacity;


  // Rehash: Iterate OLD items and place them into the NEWLY resized and zeroed iter->items
  for (size_t i = 0; i < origin_size; i++) { // Iterate up to old capacity
    mongory_table_node *node = (mongory_table_node *)origin_items[i];
    if (node) { // Only walk if there was a chain at this old slot
      mongory_table_node_walk(node, self, mongory_table_rehash_on_node);
    }
  }

  return true;
}

typedef struct mongory_table_kv_context {
  char *key;
  mongory_value *value;
} mongory_table_kv_context;

static bool mongory_table_get_on_node(mongory_table_node *node, void *acc) {
  mongory_table_kv_context *ctx = (mongory_table_kv_context *)acc;
  if (strcmp(node->key, ctx->key) == 0) {
    ctx->value = node->value;
    return false;
  }

  return true;
}

mongory_value* mongory_table_get(mongory_table *self, char *key) {
  mongory_iterable *iter = (mongory_iterable *)self->base;
  size_t index = hash_string(key) % self->capacity;
  mongory_table_node *root_node = (mongory_table_node *)mongory_iterable_get(iter, index);
  mongory_table_kv_context ctx = { key, NULL };
  mongory_table_node_walk(root_node, &ctx, mongory_table_get_on_node);
  return ctx.value;
}

static bool mongory_table_set_on_node(mongory_table_node *node, void *acc) {
  mongory_table_kv_context *ctx = (mongory_table_kv_context *)acc;
  if (strcmp(node->key, ctx->key) == 0) {
    node->value = ctx->value;
    return false;
  }

  return true;
}

bool mongory_table_set(mongory_table *self, char *key, mongory_value *value) {
  mongory_iterable *iter = (mongory_iterable *)self->base;
  size_t index = hash_string(key) % self->capacity;
  mongory_table_node *root_node =(mongory_table_node *)mongory_iterable_get(iter, index);
  mongory_table_kv_context ctx = { key, value };
  if (!mongory_table_node_walk(root_node, &ctx, mongory_table_set_on_node)) return true;

  mongory_table_node *new_node = mongory_table_node_new(self);
  if (!new_node) return false;

  new_node->key = key;
  new_node->value = value;
  new_node->next = root_node;
  mongory_iterable_set(iter, index, new_node);

  self->count++;
  if (self->count > self->capacity * MONGORY_TABLE_LOAD_FACTOR) {
    mongory_table_rehash(self);
  }
  return true;
}

bool mongory_table_del(mongory_table *self, char *key) {
  mongory_iterable *iter = (mongory_iterable *)self->base;
  size_t index = hash_string(key) % self->capacity;
  mongory_table_node **node = (mongory_table_node **)&iter->items[index];
  while (*node) {
    if (strcmp((*node)->key, key) == 0) {
      *node = (*node)->next;
      self->count--;
      return true;
    }

    node = &(*node)->next;
  }

  return false;
}

typedef struct mongory_table_each_pair_context {
  void *acc;
  mongory_table_each_pair_callback_func callback;
} mongory_table_each_pair_context;

static bool mongory_table_each_pair_on_node(mongory_table_node *node, void *acc) {
  mongory_table_each_pair_context *ctx = (mongory_table_each_pair_context *)acc;
  return ctx->callback(node->key, node->value, ctx->acc);
}

static bool mongory_table_each_pair_on_root(void *node_ptr, void *acc) {
  mongory_table_node *node = (mongory_table_node *)node_ptr;
  return mongory_table_node_walk(node, acc, mongory_table_each_pair_on_node);
}

bool mongory_table_each_pair(mongory_table *self, void *acc, mongory_table_each_pair_callback_func callback) {
  mongory_table_each_pair_context each_ctx = { acc, callback };
  return mongory_iterable_each(self->base, &each_ctx, mongory_table_each_pair_on_root);
}

mongory_table* mongory_table_new(mongory_memory_pool *pool) {
  mongory_iterable *iter = mongory_iterable_new(pool);
  if (!iter) {
    return NULL;
  }

  mongory_table *table = pool->alloc(pool->ctx, sizeof(mongory_table));
  if (!table) {
    return NULL;
  }

  table->pool = pool;
  table->base = iter;
  table->capacity = MONGORY_TABLE_INIT_SIZE;
  table->count = 0;

  // Ensure iterable has the same capacity as the table and initialize items to NULL
  mongory_iterable *base_iter = (mongory_iterable *)table->base;
  if (!mongory_iterable_resize(base_iter, table->capacity)) {
    // TODO: Handle memory allocation failure during resize, maybe free(table) and return NULL
    return NULL;
  }
  for (size_t i = 0; i < table->capacity; ++i) {
    base_iter->items[i] = NULL;
  }
  base_iter->count = table->capacity; // Critical for mongory_iterable_get to work correctly

  table->each = mongory_table_each_pair;
  table->get = mongory_table_get;
  table->set = mongory_table_set;
  table->del = mongory_table_del;

  return table;
}

