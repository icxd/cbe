#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

#define ARENA_DEBUG

typedef struct arena {
  struct arena *next;
  size_t capacity, size;
  uint8_t *data;
} arena_t;

void _a_init(size_t);
void *a_alloc(size_t);
void *a_realloc(void *, size_t, size_t);
void a_reset();
void a_free();

#define a_init(size)                                                           \
  do {                                                                         \
    _a_init(size);                                                             \
    atexit(a_free);                                                            \
  } while (0)

#endif // ARENA_H
