#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>
#include <array>
#include <string>


extern const uint32_t WIDTH;
extern const uint32_t HEIGHT;
extern const int MAX_FRAMES_IN_FLIGHT;
extern const std::vector<const char*> validationLayers;
extern const std::vector<const char*> deviceExtensions;
extern const bool enableValidationLayers;

extern PFN_vkCreateAccelerationStructureKHR pfnCreateAccelerationStructureKHR;
extern PFN_vkGetAccelerationStructureBuildSizesKHR pfnGetAccelerationStructureBuildSizesKHR;
extern PFN_vkCmdBuildAccelerationStructuresKHR pfnCmdBuildAccelerationStructuresKHR;
extern PFN_vkDestroyAccelerationStructureKHR pfnDestroyAccelerationStructureKHR;
extern PFN_vkGetBufferDeviceAddressKHR pfnGetBufferDeviceAddressKHR;
extern PFN_vkGetAccelerationStructureDeviceAddressKHR pfnGetAccelerationStructureDeviceAddressKHR;
extern PFN_vkGetRayTracingShaderGroupHandlesKHR pfnGetRayTracingShaderGroupHandlesKHR;
extern PFN_vkCreateRayTracingPipelinesKHR pfnCreateRayTracingPipelinesKHR;
extern PFN_vkCmdTraceRaysKHR pfnCmdTraceRaysKHR;

extern const VkFormat VERTEX_FORMAT;

extern uint32_t NUM_TEXTURES;

typedef uint32_t INDEX_TYPE;
