#pragma once

#include <cstdint>

#define VK_NULL_HANDLE nullptr

struct VkBuffer_T; using VkBuffer = VkBuffer_T*;
struct VkDeviceMemory_T; using VkDeviceMemory = VkDeviceMemory_T*;
struct VkAccelerationStructureKHR_T; using VkAccelerationStructureKHR = VkAccelerationStructureKHR_T*;
struct VkDevice_T; using VkDevice = VkDevice_T*;

using VkGeometryInstanceFlagsKHR = uint32_t;
