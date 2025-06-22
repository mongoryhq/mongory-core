#ifndef MONGORY_ARRAY
#define MONGORY_ARRAY
#include "mongory-core/foundations/memory_pool.h"
#include "mongory-core/foundations/value.h"
#include <stdbool.h>

typedef struct mongory_array mongory_array; // alias

typedef bool (*mongory_array_callback_func)(mongory_value *item,
                                            void *acc); // callback function
typedef bool (*mongory_array_each_func)(
    mongory_array *self, void *acc,
    mongory_array_callback_func func); // each function
typedef bool (*mongory_array_push_func)(mongory_array *self,
                                        mongory_value *value); // push function
typedef mongory_value *(*mongory_array_get_func)(mongory_array *self,
                                                 size_t index); // get function
typedef bool (*mongory_array_set_func)(mongory_array *self, size_t index,
                                       mongory_value *value); // set function

mongory_array *mongory_array_new(mongory_memory_pool *pool); // create new array

struct mongory_array {
  size_t count;                 // count
  mongory_memory_pool *pool;    // memory pool
  mongory_array_each_func each; // each function
  mongory_array_push_func push; // push function
  mongory_array_get_func get;   // get function
  mongory_array_set_func set;   // set function
};

#endif
