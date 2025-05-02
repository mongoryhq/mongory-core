#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <mongory-core/foundations/memory_pool.h>

static jmp_buf jmp;

void signal_handler(int sig) {
  printf("[test] Caught signal %d: likely double free\n", sig);
  longjmp(jmp, 1);
}

int main() {
  signal(SIGABRT, signal_handler);

  mongory_memory_pool *pool = mongory_memory_pool_new();

  printf("Pool size when initial: %zu \n", pool->chunk_size);
  for (int i = 0; i < 1000; i++) {
    pool->alloc(pool, sizeof(char[6]));
  }

  printf("Pool size after 1000 alloc: %zu\n", pool->chunk_size);
  char *string = pool->alloc(pool, sizeof(char[6]));
  string[0] = 'a';
  string[1] = 'b';
  string[2] = 'c';
  string[3] = 'd';
  string[4] = '\0';
  printf("Get alloc %s \n", string);
  
  pool->free(pool);
  if (setjmp(jmp) == 0) {
    free(string);
    printf("You are NOT REALLY FREE!!\n");
  } else {
    printf("Ptr is totally freed.\n");
  }

}
