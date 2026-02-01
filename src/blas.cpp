#include <vulkan/vulkan_core.h>

#include "blas.h"
#include "globals.h"
#include "renderer.h"

void Blas::create(VkDevice device,
                  const BlasInput& input,
                  Renderer& state) {

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexStride = input.vertexStride;
    geometry.geometry.triangles.vertexData.deviceAddress = input.vertexAddress;
    geometry.geometry.triangles.vertexFormat = input.vertexFormat;
    geometry.geometry.triangles.maxVertex = input.vertexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = input.indexAddress;
    geometry.geometry.triangles.transformData.deviceAddress = 0;

    VkAccelerationStructureGeometryKHR* geometries[] = { &geometry };


    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.ppGeometries = nullptr;
    buildInfo.pGeometries = &geometry;
    buildInfo.pNext = NULL;

    VkAccelerationStructureBuildSizesInfoKHR buildSizes{};
    buildSizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    uint32_t primitiveCount = input.indexCount / 3;

    pfnGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &buildSizes
        );

    state.createBuffer(
        buildSizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ,
        buffer,
        memory
        );

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = buffer;
    createInfo.size = buildSizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    createInfo.offset = 0;
    createInfo.createFlags = 0;
    createInfo.deviceAddress = 0;

    if (pfnCreateAccelerationStructureKHR(device, &createInfo, nullptr, &handle) != VK_SUCCESS) {
        throw std::runtime_error("failed creating BLAS");
    }
    buildInfo.dstAccelerationStructure = handle;

    VkBuffer scratchBuffer;
    VkDeviceMemory scratchBufferMemory;
    state.createBuffer(
        buildSizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer,
       scratchBufferMemory
    );

    VkBufferDeviceAddressInfoKHR scratchAddrInfo{};
    scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    scratchAddrInfo.buffer = scratchBuffer;
    VkDeviceAddress scratchAddress = pfnGetBufferDeviceAddressKHR(device, &scratchAddrInfo);

    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfos[] = { &rangeInfo };

    VkCommandBuffer cmdBuf_ = state.beginSingleTimeCommands();
    pfnCmdBuildAccelerationStructuresKHR(cmdBuf_, 1, &buildInfo, pRangeInfos);
    state.endSingleTimeCommands(cmdBuf_);

    vkDestroyBuffer(device, scratchBuffer, nullptr);
    vkFreeMemory(device, scratchBufferMemory, nullptr);
}

void Blas::destroy(VkDevice device) {
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, memory, nullptr);
    pfnDestroyAccelerationStructureKHR(device, handle, nullptr);
}

