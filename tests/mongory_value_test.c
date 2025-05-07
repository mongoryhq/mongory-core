#include <stdlib.h>
#include <stdio.h>
#include <mongory-core/foundations/memory_pool.h>
#include <mongory-core.h>

int main() {
  mongory_memory_pool *pool = mongory_memory_pool_new();

  mongory_value *value_b = mongory_value_wrap_b(pool, true);

  printf("Mongory value_b, type is %s, and value_b is %i\n",
    mongory_type_to_string(value_b),
    (int)*(bool *)mongory_value_extract(value_b)
  );

  mongory_value *value_i = mongory_value_wrap_i(pool, 123);

  printf("Mongory value_i, type is %s, and value_i is %i\n",
    mongory_type_to_string(value_i),
    *(int *)mongory_value_extract(value_i)
  );

  mongory_value *value_d = mongory_value_wrap_d(pool, 0.123);

  printf("Mongory value_d, type is %s, and value_d is %f\n",
    mongory_type_to_string(value_d),
    *(double *)mongory_value_extract(value_d)
  );

  mongory_value *value_s = mongory_value_wrap_s(pool, "Hello");

  printf("Mongory value_s, type is %s, and value_s is %s\n",
    mongory_type_to_string(value_s),
    *(char **)mongory_value_extract(value_s)
  );

  mongory_value *value_a = mongory_value_wrap_a(pool, mongory_array_new(pool));

  printf("Mongory value_a, type is %s\n",
    mongory_type_to_string(value_a)
  );

  mongory_table *table = pool->alloc(pool, sizeof(mongory_table));
  mongory_value *value_t = mongory_value_wrap_t(pool, table);

  printf("Mongory value_t, type is %s\n",
    mongory_type_to_string(value_t)
  );

  pool->free(pool);
}
