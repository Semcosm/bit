#include "bit/arena.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct BitArenaChunk {
    struct BitArenaChunk *next;
    size_t capacity;
    size_t used;
    unsigned char data[];
} BitArenaChunk;

struct BitArena {
    BitArenaChunk *head;
};

static size_t bit_align_up(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

BitArena *bit_arena_create(void) {
    BitArena *arena = (BitArena *)malloc(sizeof(BitArena));

    if (!arena) {
        return NULL;
    }

    arena->head = NULL;
    return arena;
}

void *bit_arena_alloc(BitArena *arena, size_t size) {
    BitArenaChunk *chunk;
    size_t alignment = sizeof(void *);
    size_t offset;

    if (!arena) {
        return NULL;
    }

    if (size == 0) {
        size = 1;
    }

    chunk = arena->head;
    if (chunk) {
        offset = bit_align_up(chunk->used, alignment);
        if (offset + size <= chunk->capacity) {
            void *ptr = chunk->data + offset;
            chunk->used = offset + size;
            return ptr;
        }
    }

    {
        size_t capacity = size > 4096 ? size : 4096;
        BitArenaChunk *new_chunk = (BitArenaChunk *)malloc(sizeof(BitArenaChunk) + capacity);

        if (!new_chunk) {
            return NULL;
        }

        new_chunk->next = arena->head;
        new_chunk->capacity = capacity;
        new_chunk->used = size;
        arena->head = new_chunk;
        return new_chunk->data;
    }
}

void bit_arena_destroy(BitArena *arena) {
    BitArenaChunk *chunk;

    if (!arena) {
        return;
    }

    chunk = arena->head;
    while (chunk) {
        BitArenaChunk *next = chunk->next;
        free(chunk);
        chunk = next;
    }

    free(arena);
}
