#ifndef BIT_ARENA_H
#define BIT_ARENA_H

#include <stddef.h>

typedef struct BitArena BitArena;

BitArena *bit_arena_create(void);
void *bit_arena_alloc(BitArena *arena, size_t size);
void bit_arena_destroy(BitArena *arena);

#endif
