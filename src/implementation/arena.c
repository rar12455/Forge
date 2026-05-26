/*
 * Copyright (C) 2026 rar
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} arena_t;

/* Functions to manage the arena */
arena_t arena_init(size_t capacity);
void*   arena_alloc(Arena* arena, size_t size);
void    arena_free(Arena* arena);

arena_t 
arena_init(size_t capacity) 
{
        arena_t arena;
        arena.buffer = malloc(capacity);
        if (!arena.buffer) {
                fprintf(stderr,"error: failed to allocate memory.");
                exit(1);
        }
        arena.capacity = capacity;
        arena.offset = 0;
        return arena;
}

// W.I.P
