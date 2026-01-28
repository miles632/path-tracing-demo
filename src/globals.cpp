#include "globals.h"

#include <vulkan/vulkan_core.h>

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

std::array<std::string, NUM_MESHES> meshPaths = {"teapot.obj", "sphere.obj", "sphere.obj"};
std::array<std::string, NUM_TEXTURES> texturePaths = {};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

PFN_vkCreateAccelerationStructureKHR pfnCreateAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR pfnGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR pfnCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR pfnDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetBufferDeviceAddressKHR pfnGetBufferDeviceAddressKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR pfnGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR pfnGetRayTracingShaderGroupHandlesKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR pfnCreateRayTracingPipelinesKHR = nullptr;
PFN_vkCmdTraceRaysKHR pfnCmdTraceRaysKHR = nullptr;


const VkFormat VERTEX_FORMAT = VK_FORMAT_R32G32B32_SFLOAT;