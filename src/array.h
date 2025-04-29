#ifndef MONGORY_ARRAY
#define MONGORY_ARRAY
typedef struct mongory_array {
  int size;
  int capacity;
  void *items;
} mongory_array;

#endif