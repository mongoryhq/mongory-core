#ifndef MONGORY_TABLE_H
#define MONGORY_TABLE_H
#include "mongory-core/foundations/array.h"
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include <stdbool.h>
#include <stddef.h>

struct mongory_table;                       // forward declaration
typedef struct mongory_table mongory_table; // alias
typedef bool (*mongory_table_each_pair_callback_func)(
    char *key, mongory_value *value, void *acc); // each pair callback function
typedef mongory_value *(*mongory_table_get_func)(mongory_table *self,
                                                 char *key); // get function
typedef bool (*mongory_table_set_func)(mongory_table *self, char *key,
                                       mongory_value *value); // set function
typedef bool (*mongory_table_each_func)(
    mongory_table *self, void *acc,
    mongory_table_each_pair_callback_func callback); // each function
typedef bool (*mongory_table_del_func)(mongory_table *self,
                                       char *key); // delete function

mongory_table *mongory_table_new(mongory_memory_pool *pool); // create new table

struct mongory_table {
  mongory_memory_pool *pool;    // memory pool
  size_t count;                 // read only!
  mongory_table_get_func get;   // get function
  mongory_table_each_func each; // each function
  mongory_table_set_func set;   // set function
  mongory_table_del_func del;   // delete function
};

#endif // MONGORY_TABLE_H
