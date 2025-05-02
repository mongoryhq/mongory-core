#ifndef MONGORY_ARRAY
#define MONGORY_ARRAY
#include <stdbool.h>

struct mongory_array;
typedef struct mongory_array mongory_array;
typedef bool (*mongory_array_callback_func)(void *item, void *acc);
typedef bool (*mongory_array_each_func)(mongory_array *self, void *acc, mongory_array_callback_func func);
typedef bool (*mongory_array_push_func)(mongory_array *self, mongory_memory_pool *pool, mongory_value *value);
typedef mongory_value (*mongory_array_get_func)(mongory_array *self, size_t index);
typedef bool (*mongory_array_set_func)(mongory_array *self, mongory_memory_pool *pool, size_t index, mongory_value *value);

mongory_array* mongory_array_new(mongory_memory_pool *pool);

struct mongory_array {
  void *base;
  mongory_array_each_func each;
  mongory_array_push_func push;
  mongory_array_get_func get;
  mongory_array_set_func set;
};

#endif
