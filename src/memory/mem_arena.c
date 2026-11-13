#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "memory/mem_arena.h"
#include "utils/types.h"

mem_arena *create_arena(u64 capacity, u64 align) {
    mem_arena *arena = malloc(capacity);

    arena->capacity = capacity;
    arena->pos = ARENA_BASE_POS;
    arena->align = align;

    return arena;
}

void arena_clear(mem_arena *arena) {
    arena->pos = ARENA_BASE_POS;
}

void arena_destroy(mem_arena *arena) {
    free(arena);
}

void *arena_push(mem_arena *arena, u64 size) {
    u64 pos_aligned = ALIGN_UP_POW2(arena->pos, arena->align);
    u64 new_pos = pos_aligned + size;

    if(new_pos > arena->capacity) {
        fprintf(stderr, "Arena capacity reached!\n");

        return NULL;
    }

    arena->pos = new_pos;

    u8* out = (u8*)arena + pos_aligned;

    return out;
}

bool arena_is_last(mem_arena *arena, void *ptr, u64 size) {
    u8 *alloc_end = (u8*)ptr + size;
    u8 *arena_end = (u8*)arena + arena->pos;

    return alloc_end == arena_end;
}

void *arena_realloc(mem_arena *arena, void *ptr, u64 old_size, u64 new_size) {
    if(ptr == NULL) {
        return NULL;
    }

    if(arena_is_last(arena, ptr, old_size)) {
        u64 new_pos = arena->pos + (new_size - old_size);
        if(new_pos > arena->capacity) {
            fprintf(stderr, "Arena capacity reached! Extending from %lu to %lu\n", arena->capacity, arena->capacity * 2);
            u64 new_capacity = arena->capacity * 2;
            arena->capacity = new_capacity;
            // TODO: don't use realloc on arenas???
            void *new_ptr = realloc(arena, arena->capacity);
            if(new_ptr == NULL) {
                fprintf(stderr, "Couldn't realloc arena\n");
                return NULL;
            };
            return new_ptr;
        }

        arena->pos = new_pos;
        return ptr;
    } else {
        void *new_ptr = arena_push(arena, new_size);
        memcpy(new_ptr, ptr, old_size);
        return new_ptr;
    }
}
