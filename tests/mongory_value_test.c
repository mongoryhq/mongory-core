#include <stdlib.h>
#include <stdio.h>
#include <mongory-core.h>

int main() {
  mongory_value *value = mongory_value_wrap_s("Hello");

  printf("Mongory value, type is %s, and value is %s\n",
    mongory_type_to_string(value->type),
    *(char **)mongory_value_extract(value)
  );
}
