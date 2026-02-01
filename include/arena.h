#pragma once
#include <cstddef>

struct Arena {
    size_t capacity;
    size_t offset;
    size_t alignment;

    std::byte* data;
};

size_t alignUp(size_t n);
void arenaInit(struct Arena* arena, size_t size);
std::byte* arenaAlloc(struct Arena* arena, size_t size);
void arenaReset(struct Arena* arena);
void arenaFree(struct Arena* arena);
void arenaGrow(struct Arena* arena, size_t requiredSize);

