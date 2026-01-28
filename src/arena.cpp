#include "../include/arena.h"

#include <iostream>
#include <stdexcept>

size_t alignUp(size_t n, size_t alignment) {
    return (n + alignment - 1) & ~(alignment - 1);
}

void arenaInit(struct Arena *arena, size_t size) {
    arena->alignment = 1;

    void* memory = aligned_alloc(arena->alignment, alignUp(size, arena->alignment));
    if (!memory) throw std::runtime_error("malloc failed allocating for arena");

    arena->data = reinterpret_cast<std::byte*>(memory);
    arena->capacity = size;
    arena->offset = 0;
}

std::byte* arenaAlloc(struct Arena *arena, size_t size) {
    size_t alignedOffset = alignUp(arena->offset, arena->alignment);
    size_t nextOffset = alignedOffset + size;

    if (nextOffset > arena->capacity) {
#ifdef DEBUG
        fprintf(stderr,
            "ARENA OVERFLOW: request=%zu   offset=%zu   capacity=%zu   alignedOffset=%zu   next=%zu\n",
            size, arena->offset, arena->capacity, alignedOffset, nextOffset);
#endif

        arenaGrow(arena, size);
    }

    std::byte *ptr = arena->data + alignedOffset;
    arena->offset = nextOffset;
    return ptr;
}

void arenaGrow(struct Arena *arena, size_t requiredSize) {
    size_t newCapacity= arena->capacity * 2;
    while (arena->offset + requiredSize > newCapacity) {
        newCapacity *= 2;
    }

    void* newMem = aligned_alloc(arena->alignment, alignUp(newCapacity, arena->alignment));
    memcpy(newMem, arena->data, arena->offset);

    free(arena->data);
    arena->data = reinterpret_cast<std::byte*>(newMem);
    arena->capacity = newCapacity;
}


void arenaReset(struct Arena *arena) {
#ifdef DEBUG
    memset(arena->data, 0xCD, arena->capacity);
#endif
    arena->offset = 0;
}

void arenaFree(struct Arena *arena) {
    free(arena->data);
    arena->data = nullptr;
    arena->capacity = 0;
    arena->offset = 0;
}


