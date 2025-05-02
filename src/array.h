#ifndef MONGORY_ARRAY
#define MONGORY_ARRAY
#include <stdbool.h>

struct mongory_array;
typedef struct mongory_array mongory_array;
typedef bool (*mongory_array_callback_func)(void *item, void *acc);
bool mongory_array_each(mongory_array *self, void *acc, mongory_array_callback_func func);

#endif