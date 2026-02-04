#pragma once

#include "foward_declarations.h"
#include <vector>
#include <glm/mat3x4.hpp>

struct Renderer;

struct TlasInstance {
    glm::mat3x4 transform;
    uint32_t instanceCustomIndex : 24;
    uint32_t mask : 8;
    uint32_t instanceShaderBindingTableRecordOffset : 24;
    VkGeometryInstanceFlagsKHR flags : 8;
    uint64_t blasDeviceAddress;
};


class Tlas {
public:
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;

    void create(VkDevice device,
                const std::vector<TlasInstance>& instances,
                Renderer& state
                );

    void destroy(VkDevice device);
};