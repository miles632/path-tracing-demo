#include "../include/arena.h"

#include <stdexcept>

// this aligns to a multiple of 8
size_t alignUp(size_t n, size_t alignment) {
    return (n + alignment - 1) & ~(alignment - 1);
}

void arenaInit(struct Arena *arena, size_t size, size_t alignment) {
    void* memory = malloc(size);
    if (!memory) throw std::runtime_error("malloc failed allocating for arena");

    arena->data = reinterpret_cast<std::byte*>(memory);
    arena->capacity = size;
    arena->offset = 0;
    arena->alignment = alignment;
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
        return nullptr;
    }

    std::byte *ptr = arena->data + alignedOffset;
    arena->offset = nextOffset;
    return ptr;
}

void arenaReset(struct Arena *arena) {
    arena->offset = 0;
}

void arenaFree(struct Arena *arena) {
    free(arena->data);
    arena->data = nullptr;
    arena->capacity = 0;
    arena->offset = 0;
}


