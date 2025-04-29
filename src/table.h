#ifndef MONGORY_TABLE
#define MONGORY_TABLE
typedef struct mongory_table {
  int size;
  int capacity;
  void *buckets;
} mongory_table;

#endif