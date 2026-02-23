#pragma once

#include "vertex.h"

struct Renderer; //forward declaration

struct BlasInput {
    VkDeviceAddress vertexAddress;
    VkDeviceAddress indexAddress;

    uint32_t vertexCount;
    uint32_t indexCount;

    size_t vertexOffset;
    size_t indexOffset;

    VkFormat vertexFormat;
    VkIndexType indexType = VK_INDEX_TYPE_UINT32;
    const uint32_t vertexStride = sizeof(Vertex);
};

class Blas {
public:
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;

    void create(VkDevice device,
                const BlasInput& input,
                Renderer& state);

    void destroy(VkDevice device);
};