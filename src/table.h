typedef struct mongory_table {
  int size;
  int capacity;
  void *buckets;
} mongory_table;
