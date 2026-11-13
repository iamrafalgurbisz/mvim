#pragma once

#include "utils/types.h"

#define ARENA_BASE_POS (sizeof(mem_arena))
#define ARENA_ALIGN (sizeof(void*))
#define ALIGN_UP_POW2(n, p) (((u64)(n) + ((u64)(p - 1)) & (~((u64)(p) - 1))))

typedef struct {
    u64 capacity;
    u64 align;
    u64 pos;
} mem_arena;

mem_arena *create_arena(u64 capacity, u64 align);

void arena_clear(mem_arena *arena);

void arena_destroy(mem_arena *arena);

void *arena_push(mem_arena *arena, u64 size);

void *arena_realloc(mem_arena *arena, void *ptr, u64 old_size, u64 new_size);
