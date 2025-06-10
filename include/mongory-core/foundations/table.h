#ifndef MONGORY_TABLE_H
#define MONGORY_TABLE_H
#include <stdbool.h>
#include <stddef.h>
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"

struct mongory_table;
typedef struct mongory_table mongory_table;
typedef bool (*mongory_table_each_pair_callback_func)(const char *key, mongory_value *value, void *acc);
typedef mongory_value* (*mongory_table_get_func)(mongory_table *self, char *key);
typedef bool (*mongory_table_set_func)(mongory_table *self, char *key, mongory_value *value);
typedef bool (*mongory_table_each_func)(mongory_table *self, void *acc, mongory_table_each_pair_callback_func callback);
typedef bool (*mongory_table_del_func)(mongory_table *self, char *key);

mongory_table* mongory_table_new(mongory_memory_pool *pool);

struct mongory_table {
  void *base;  // mongory_iterable*
  mongory_memory_pool *pool;
  size_t capacity;
  size_t count;
  mongory_table_get_func get;
  mongory_table_each_func each;
  mongory_table_set_func set;
  mongory_table_del_func del;
};

#endif // MONGORY_TABLE_H
