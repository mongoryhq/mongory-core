#ifndef MONGORY_TABLE
#define MONGORY_TABLE
#include <stdbool.h>

struct mongory_table;
typedef struct mongory_table mongory_table;
typedef bool (mongory_table_callback_func)(char *key, mongory_value *value, void *acc);
bool mongory_table_each(mongory_table *self, void *acc, mongory_table_callback_func func);

#endif
