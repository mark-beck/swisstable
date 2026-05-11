#pragma once

#include <stdint.h>
#include <stdlib.h>


typedef _Arena struct {
    void *memoryblock;
    uint64_t bytes_allocated;
    uint64_t max_size
    _Arena *next_node;
} Arena;

Arena *arena_create(uint64_t size) {
    void block = malloc(size);
    Arena arena = malloc(sizeof(Arena));
    arena = {
        .memoryblock = block,
        .bytes_allocated = 0,
        .max_bytes = size,
        .next_node = NULL
    };
    return arena;
}

void *arena_alloc(Arena *arena, uint64_t size) {
    if (arena->bytes_allocated + size >= arena->max_size) {
        Arena *node = arena_create(arena->max_size);
        arena->next_node = node;
        return arena_alloc(node, size);
    }

    void *ptr = arena->memoryblock + arena->bytes_allocated;
    arena->bytes_allocated += size;
    return ptr;
}

void arena_destroy(Arena *arena) {
    if (arena->next_node)
        arena_destroy(arena->next_node);

    free(arena->memoryblock);
    free(arena);
}

