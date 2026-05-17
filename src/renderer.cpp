#include "renderer.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <cstring>

//#include "vertex.h"
#include "host_device.h"
#include "globals.h"

#include "blas.h"
#include "tiny_obj_loader.h"
#include "stb_image/stb_image.h"
#include "tlas.h"

#include "vulkan/vulkan.h"
#include "tiny_gltf.h"

// because the CLion linter doesnt automatically search for includes in /external for whatever reason
#include <filesystem>

#include "../external/imgui/imgui.h"
#include "../external/imgui/backends/imgui_impl_vulkan.h"
#include "../external/imgui/backends/imgui_impl_glfw.h"
#include "gui.h"

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
    ) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
    ) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void Renderer::initWindow() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "renderer", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);

        glfwSetKeyCallback(window, &keyInputCallback);
        glfwSetCursorPosCallback(window, &mouseInputCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

void Renderer::initVulkan() {
    std::cout << "[Init] Creating Vulkan instance\n";
    createInstance();

    std::cout << "[Init] Setting up debug messenger\n";
    setupDebugMessenger();

    std::cout << "[Init] Creating surface\n";
    createSurface();

    std::cout << "[Init] Selecting physical device\n";
    pickPhysicalDevice();

    std::cout << "[Init] Creating logical device\n";
    createLogicalDevice();

    std::cout << "[Init] Creating swapchain\n";
    createSwapChain();

    std::cout << "[Init] Creating swapchain image views\n";
    createImageViews();

    std::cout << "[Init] Creating command pool\n";
    createCommandPool();

    std::cout << "[Init] Creating ray tracing storage image\n";
    createStorageImage_RT();

    std::cout << "[Init] Creating destination image\n";
    createDstImage_RT();

    std::cout << "[Init] Creating camera\n";
    createCamera();

    std::cout << "[Init] Loading GLTF meshes\n";
    createScene_GLTF();

    std::cout << "[Init] Creating vertex buffer\n";
    createVertexBuffer();

    std::cout << "[Init] Creating index buffer\n";
    createIndexBuffer();

    std::cout << "[Init] Creating offset buffer\n";
    createOffsetBuffer();

    std::cout << "[Init] Creating material buffer\n";
    createMaterialBuffer();

    std::cout << "[Init] Creating uniform buffers\n";
    createUniformBuffers();

    std::cout << "[Init] Building BLAS\n";
    createBottomLevelAccelerationStructures();

    std::cout << "[Init] Building TLAS\n";
    createTopLevelAccelerationStructure();

    std::cout << "[Init] Creating descriptor set layout\n";
    createDescriptorSetLayout_RT();

    std::cout << "[Init] Creating ray tracing pipeline\n";
    createPipeline_RT();

    std::cout << "[Init] Creating descriptor pool\n";
    createDescriptorPool_RT();

    std::cout << "[Init] Allocating descriptor set\n";
    createDescriptorSet_RT();

    std::cout << "[Init] Creating command buffers\n";
    createCommandBuffers();

    std::cout << "[Init] Creating shader binding table\n";
    createShaderBindingTable();

    std::cout << "[Init] Creating synchronization objects\n";
    createSyncObjects();

    std::cout << "[Init] Vulkan initialization complete\n";
}

void Renderer::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        float currentFrameT = glfwGetTime();
        float deltaTime = currentFrameT - lastFrameT;
        lastFrameT = currentFrameT;

        glfwPollEvents();

        if (pendingSceneRecreate) {
            recreateScene_GLTF();
            pendingSceneRecreate = false;
        }

        if (camera.move(deltaTime)) frameCount = 0;
        drawFrame();
        frameCount++;
    }

    vkDeviceWaitIdle(device);
}

void Renderer::keyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    Renderer* rd = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            if (key == GLFW_KEY_ESCAPE) {
                rd->cursorCaptured = !rd->cursorCaptured;
                glfwSetInputMode(window, GLFW_CURSOR, rd->cursorCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                glfwSetCursorPos(window, rd->swapChainExtent.width/2, rd->swapChainExtent.height/2);
            } else { // other keys are for moving the camera
                rd->camera.keys[key] = true;
            }
        } else if (action == GLFW_RELEASE) {
            rd->camera.keys[key] = false;
        }
    }
}

void Renderer::mouseInputCallback(GLFWwindow *window, double xpos, double ypos) {
    Renderer* rd = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if (!rd->cursorCaptured)
        return;

    static float lastX = 400.0f;
    static float lastY = 300.0f;
    static bool firstMouse = true;

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastX;
    float yOffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    if (rd->camera.mouse(xOffset, yOffset)) {
        rd->frameCount = 0;
    }
}

void Renderer::drawFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult swapchainResult = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (swapchainResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (swapchainResult != VK_SUCCESS && swapchainResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // if that image is being used by another frame, wait on its fence
    if (inFlightImages[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &inFlightImages[imageIndex], VK_TRUE, UINT64_MAX);
    }
    // mark image as being used by current frames fence
    inFlightImages[imageIndex] = inFlightFences[currentFrame];

    updateUniformBuffer(currentFrame);

    // unsignal
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    VkCommandBuffer currentCmdBuf = commandBuffers[currentFrame];
    vkResetCommandBuffer(currentCmdBuf, 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(currentCmdBuf, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer");
    }
    raytrace(currentCmdBuf, imageIndex);

    // draw gui
    if (guiHandle != nullptr) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.framebuffer = guiHandle->framebuffers[imageIndex];
        renderPassInfo.renderPass = imguiRenderPass;
        renderPassInfo.renderArea.offset = {0,0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 0;
        renderPassInfo.pClearValues = nullptr;

        vkCmdBeginRenderPass(currentCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        guiHandle->draw(this, currentCmdBuf);
        vkCmdEndRenderPass(currentCmdBuf);
    }
    vkEndCommandBuffer(currentCmdBuf);


    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[imageIndex]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult queueSubmitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
    if (queueSubmitResult != VK_SUCCESS) {
        throw std::runtime_error("vkQueueSubmit failed! " + std::to_string(queueSubmitResult));
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult queuePresentResult = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (queuePresentResult != VK_SUCCESS) {
        throw std::runtime_error("failed to present to queue");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}




void Renderer::createVertexBuffer() {
    std::byte* vertexData = vertexArena.data;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
        vertexArena.capacity,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* stagingData;
    vkMapMemory(device, stagingBufferMemory, 0, vertexArena.capacity, 0, &stagingData);
    memcpy(stagingData, vertexData, vertexArena.offset);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(
        vertexArena.capacity,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

    copyBuffer(stagingBuffer, vertexBuffer, vertexArena.capacity);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = vertexBuffer;
    vertexBufferAddress = pfnGetBufferDeviceAddressKHR(device, &addrInfo);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
void Renderer::createIndexBuffer() {
    std::byte* indexData = indexArena.data;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
        vertexArena.capacity,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* stagingData;
    vkMapMemory(device, stagingBufferMemory, 0, indexArena.capacity, 0, &stagingData);
    memcpy(stagingData, indexData, indexArena.offset);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(
     indexArena.capacity,
     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
     indexBuffer,
     indexBufferMemory
     );

    copyBuffer(stagingBuffer, indexBuffer, indexArena.capacity);

    VkBufferDeviceAddressInfo bufferAddressInfo{};
    bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferAddressInfo.buffer = indexBuffer;
    indexBufferAddress = pfnGetBufferDeviceAddressKHR(device, &bufferAddressInfo);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
void Renderer::createOffsetBuffer() {
    std::byte* offsetData = offsetArena.data;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(
        offsetArena.capacity,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* stagingData;
    vkMapMemory(device, stagingBufferMemory, 0, offsetArena.capacity, 0, &stagingData);
    memcpy(stagingData, offsetData, offsetArena.offset);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(
     offsetArena.capacity,
     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
     offsetBuffer,
     offsetBufferMemory
     );

    copyBuffer(stagingBuffer, offsetBuffer, offsetArena.capacity);

    VkBufferDeviceAddressInfo bufferAddressInfo{};
    bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferAddressInfo.buffer = offsetBuffer;
    offsetBufferAddress = pfnGetBufferDeviceAddressKHR(device, &bufferAddressInfo);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

VkCommandBuffer Renderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate single time command buffer") ;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin single time command buffer");
    }

    return commandBuffer;
}

void Renderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}


void Renderer::cleanup() {
    vkDeviceWaitIdle(device);

    if (sbtBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, sbtBuffer, nullptr);
        sbtBuffer = VK_NULL_HANDLE;
    }
    if (sbtBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, sbtBufferMemory, nullptr);
        sbtBufferMemory = VK_NULL_HANDLE;
    }

    if (offsetBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, offsetBuffer, nullptr);
        offsetBuffer = VK_NULL_HANDLE;
    }

    if (offsetBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, offsetBufferMemory, nullptr);
        offsetBufferMemory = VK_NULL_HANDLE;
    }

    if (indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, indexBuffer, nullptr);
        indexBuffer = VK_NULL_HANDLE;
    }
    if (indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, indexBufferMemory, nullptr);
        indexBufferMemory = VK_NULL_HANDLE;
    }

    if (vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vertexBufferMemory = VK_NULL_HANDLE;
    }

    for (size_t i = 0; i < uniformBuffers.size(); ++i) {
        if (uniformBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            uniformBuffers[i] = VK_NULL_HANDLE;
        }
        if (uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            uniformBuffersMemory[i] = VK_NULL_HANDLE;
        }
    }

    for (auto& blasInstance: blasPool) {
        blasInstance.destroy(device);
    }
    tlas.destroy(device);

    free(indexArena.data);
    free(offsetArena.data);
    free(vertexArena.data);

    if (descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }

    if (graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        graphicsPipeline = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }


    for (auto iv : swapChainImageViews) {
        if (iv != VK_NULL_HANDLE) vkDestroyImageView(device, iv, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        swapChain = VK_NULL_HANDLE;
    }

    if (!commandBuffers.empty() && commandPool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, commandPool,
                             static_cast<uint32_t>(commandBuffers.size()),
                             commandBuffers.data());
        commandBuffers.clear();
    }
    if (commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }

    for (auto s : imageAvailableSemaphores) {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
    }
    imageAvailableSemaphores.clear();

    for (auto s : renderFinishedSemaphores) {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(device, s, nullptr);
    }
    renderFinishedSemaphores.clear();

    for (auto f : inFlightFences) {
        if (f != VK_NULL_HANDLE) vkDestroyFence(device, f, nullptr);
    }

    inFlightFences.clear();

    if (storageImageView_RT != VK_NULL_HANDLE) { vkDestroyImageView(device, storageImageView_RT, nullptr); storageImageView_RT = VK_NULL_HANDLE; }
    if (storageImage_RT != VK_NULL_HANDLE) { vkDestroyImage(device, storageImage_RT, nullptr); storageImage_RT = VK_NULL_HANDLE; }
    if (storageImageMemory_RT != VK_NULL_HANDLE) { vkFreeMemory(device, storageImageMemory_RT, nullptr); storageImageMemory_RT = VK_NULL_HANDLE; }

    if (dstImageView_RT != VK_NULL_HANDLE) { vkDestroyImageView(device, dstImageView_RT, nullptr); dstImageView_RT = VK_NULL_HANDLE; }
    if (dstImage_RT != VK_NULL_HANDLE) { vkDestroyImage(device, dstImage_RT, nullptr); dstImage_RT = VK_NULL_HANDLE; }
    if (dstImageMemory_RT != VK_NULL_HANDLE) { vkFreeMemory(device, dstImageMemory_RT, nullptr); dstImageMemory_RT = VK_NULL_HANDLE; }

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }

    glfwTerminate();
}

void Renderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Ray tracing demo";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 3, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;

        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

void Renderer::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface)) {
        throw std::runtime_error("failed to create window surface");
    }
}

bool Renderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0 ){
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }
    return true;
}

std::vector<const char*> Renderer::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

void Renderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData
    ) {

    std::cerr << "validation layer:" << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Renderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device_: devices) {
        if (isDeviceSuitable(device_)) {
            physicalDevice = device_;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device_) {
    QueueFamilyIndices indices = findQueueFamilies(device_);

    bool extensionsSupported = checkDeviceExtensionSupport(device_);

    // VkPhysicalDeviceFeatures supportedFeatures;
    // vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
    accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStructFeatures.pNext = &rtPipelineFeatures;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatures{};
    bufferAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferAddressFeatures.pNext = &accelStructFeatures;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &bufferAddressFeatures;

    vkGetPhysicalDeviceFeatures2(device_, &features2);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device_);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    //return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy

    return indices.isComplete() &&
            extensionsSupported &&
            swapChainAdequate &&
            features2.features.samplerAnisotropy &&
            bufferAddressFeatures.bufferDeviceAddress &&
            accelStructFeatures.accelerationStructure &&
            rtPipelineFeatures.rayTracingPipeline;
}

bool Renderer::checkDeviceExtensionSupport(VkPhysicalDevice device_) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device_, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device_, nullptr, &extension_count, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void Renderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value(),
    };

    float queuePriority = 1.0f;
    //create create info for every index
    for(uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    //VkPhysicalDeviceFeatures deviceFeatures{};
    //deviceFeatures.samplerAnisotropy = VK_TRUE;
    //deviceFeatures.sampleRateShading = VK_TRUE;

    // ray tracing pipeline features
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures{};
    rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtPipelineFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelStructFeatures{};
    accelStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelStructFeatures.accelerationStructure = VK_TRUE;
    accelStructFeatures.pNext = &rtPipelineFeatures;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddressFeatures{};
    bufferAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferAddressFeatures.bufferDeviceAddress = VK_TRUE;
    bufferAddressFeatures.pNext = &accelStructFeatures;


    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.samplerAnisotropy = VK_TRUE;
    features2.features.sampleRateShading = VK_TRUE;
    features2.pNext = &bufferAddressFeatures;

    /*
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.runtimeDescriptorArray = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.pNext = &features2;
    */

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pNext = &features2;
    createInfo.pEnabledFeatures = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    //load ray tracing related function pointers
    pfnCreateAccelerationStructureKHR =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    pfnGetAccelerationStructureBuildSizesKHR =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    pfnCmdBuildAccelerationStructuresKHR =
        (PFN_vkCmdBuildAccelerationStructuresKHR) vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    pfnDestroyAccelerationStructureKHR =
        (PFN_vkDestroyAccelerationStructureKHR) vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    pfnGetBufferDeviceAddressKHR =
        (PFN_vkGetBufferDeviceAddressKHR) vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR");
    pfnGetAccelerationStructureDeviceAddressKHR =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR) vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    pfnGetRayTracingShaderGroupHandlesKHR =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR) vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    pfnCreateRayTracingPipelinesKHR =
        (PFN_vkCreateRayTracingPipelinesKHR) vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    pfnCmdTraceRaysKHR =
        (PFN_vkCmdTraceRaysKHR) vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device_) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device_, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device_, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device_, i, surface, &presentSupport);

        if(presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device_) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device_, surface, &details.capabilities);

    uint32_t formatCounter;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device_, surface, &formatCounter, nullptr);

    if (formatCounter != 0) {
        details.formats.resize(formatCounter);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device_, surface, &formatCounter, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device_, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device_, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Renderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void Renderer::recreateSwapChain() {
    int width = 0; int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while(width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
}

void Renderer::cleanupSwapChain() {
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void Renderer::createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat);
    }
}


void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usageFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed creating vertex buffer");
    }

    VkMemoryRequirements memRequirements;

    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, propertyFlags);

    VkMemoryAllocateFlagsInfo memoryFlagsInfo{};
    if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        memoryFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memoryFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        memoryFlagsInfo.pNext = nullptr;

        allocInfo.pNext = &memoryFlagsInfo;
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed allocating memory for buffer");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    // not doing a cast causes undefined behaviour when compiling without -O0 ??????
    // TODO: why
    copyRegion.srcOffset = static_cast<uint64_t>(0);
    copyRegion.dstOffset = static_cast<uint64_t>(0);
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0;  i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)
            && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type");
}

void Renderer::createPipeline_RT() {
    auto rgenShaderCode = readFile("shaders/rgen.spv");
    auto rchitShaderCode = readFile("shaders/rchit.spv");
    auto rmissShaderCode = readFile("shaders/rmiss.spv");
    auto smissShaderCode = readFile("shaders/smiss.spv");
    //auto copyToSwapchainCode = readFile("shaders/copy.spv");

    VkShaderModule rgenModule = createShaderModule(rgenShaderCode);
    VkShaderModule rchitModule = createShaderModule(rchitShaderCode);
    VkShaderModule rmissModule = createShaderModule(rmissShaderCode);
    VkShaderModule smissModule = createShaderModule(smissShaderCode);
    //VkShaderModule copyModule = createShaderModule(copyToSwapchainCode);

    VkPipelineShaderStageCreateInfo rgenStageInfo{};
    rgenStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    rgenStageInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    rgenStageInfo.module = rgenModule;
    rgenStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo chitStageInfo{};
    chitStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    chitStageInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    chitStageInfo.module = rchitModule;
    chitStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo missStageInfo{};
    missStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    missStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    missStageInfo.module = rmissModule;
    missStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo smissStageInfo{};
    smissStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    smissStageInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    smissStageInfo.module = smissModule;
    smissStageInfo.pName = "main";



    std::array<VkPipelineShaderStageCreateInfo, 4> shaderStages = {rgenStageInfo, chitStageInfo, missStageInfo, smissStageInfo};

    VkRayTracingShaderGroupCreateInfoKHR raygenGroup{};
    raygenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenGroup.generalShader = 0; //0th in shaderStages
    raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR missGroup{};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 2;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR smissGroup{};
    smissGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    smissGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    smissGroup.generalShader = 3;
    smissGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    smissGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    smissGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingShaderGroupCreateInfoKHR hitGroup{};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = 1;
    hitGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;

    std::array shaderGroups = {raygenGroup, missGroup, smissGroup, hitGroup};

    VkPushConstantRange pcRange{};
    pcRange.stageFlags =    VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                            VK_SHADER_STAGE_MISS_BIT_KHR |
                            VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    pcRange.offset = 0;
    pcRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed creating pipeline layout");
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.pNext = nullptr;
    pipelineInfo.flags = 0;
    pipelineInfo.stageCount = 4;
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.groupCount = (uint32_t)shaderGroups.size();
    pipelineInfo.pGroups = shaderGroups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 4;
    pipelineInfo.pLibraryInfo = nullptr; // TODO: implement pipeline libraries later
    pipelineInfo.pLibraryInterface = nullptr;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (pfnCreateRayTracingPipelinesKHR(
        device, VK_NULL_HANDLE, VK_NULL_HANDLE,
        1, &pipelineInfo, nullptr, &graphicsPipeline ) != VK_SUCCESS) {
        throw std::runtime_error("failed creating graphics pipeline");
    }

    /*
    VkPipelineShaderStageCreateInfo copyToSwapchainInfo{};
    copyToSwapchainInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    copyToSwapchainInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    copyToSwapchainInfo.module = copyModule;
    copyToSwapchainInfo.pName = "main";

    VkPipelineLayoutCreateInfo computeLayoutInfo{};
    computeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayoutInfo.setLayoutCount = 1;
    computeLayoutInfo.pSetLayouts = &descriptorSetLayout;
    computeLayoutInfo.pushConstantRangeCount = 0;
    computeLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(device, &computeLayoutInfo, nullptr, &computePipelineLayout_RT) != VK_SUCCESS) {
        throw std::runtime_error("failed creating compute pipeline");
    }

    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.pNext = nullptr;
    computePipelineInfo.flags = 0;
    computePipelineInfo.stage = copyToSwapchainInfo;
    computePipelineInfo.layout = computePipelineLayout_RT;
    computePipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
        &computePipelineInfo, nullptr, &computePipeline_RT) != VK_SUCCESS) {
        throw std::runtime_error("failed creating compute pipeline");
    }
    */

    vkDestroyShaderModule(device, rchitModule, nullptr);
    vkDestroyShaderModule(device, rgenModule, nullptr);
    vkDestroyShaderModule(device, rmissModule, nullptr);
    vkDestroyShaderModule(device, smissModule, nullptr);
    //vkDestroyShaderModule(device, copyModule, nullptr);
}

void Renderer::createUniformBuffers() {
    VkDeviceSize bufSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        createBuffer(bufSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
        vkMapMemory(device, uniformBuffersMemory[i], 0, bufSize, 0, &uniformBuffersMapped[i]);
    }
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};


    ubo.model = glm::mat4(1.0f);

    ubo.view = camera.getViewMatrix();

    ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float) swapChainExtent.height, 0.1f, 50.0f);

    // flip image back up because glm is for GL clip coordinates with an inverted Y axis
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Renderer::createCamera() {
    this->camera = Camera{};
}

void Renderer::createDescriptorSet_RT() {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed allocating descriptor sets");
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {

        std::vector<VkDescriptorImageInfo> textureImgInfos;
        textureImgInfos.reserve(textures.size());

        for (const auto& texture : textures) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.view;
            assert(texture.view != VK_NULL_HANDLE);
            imageInfo.sampler = texture.sampler;

            textureImgInfos.push_back(imageInfo);
        }

        // UBO not needed for raytracing, at least for now using simple methods
        VkDescriptorBufferInfo UBOInfo{};
        UBOInfo.buffer = uniformBuffers[i];
        UBOInfo.offset = 0;
        UBOInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSetAccelerationStructureKHR accelStruct{};
        accelStruct.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accelStruct.accelerationStructureCount = 1;
        accelStruct.pAccelerationStructures = &tlas.handle;

        VkDescriptorImageInfo storageImageInfo{};
        storageImageInfo.imageView = storageImageView_RT;
        storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        storageImageInfo.sampler = VK_NULL_HANDLE;

        VkDescriptorImageInfo dstImageInfo{};
        dstImageInfo.imageView = dstImageView_RT;
        dstImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        dstImageInfo.sampler = VK_NULL_HANDLE;

        VkDescriptorBufferInfo storageBufInfoVertex{};
        storageBufInfoVertex.buffer = vertexBuffer;
        storageBufInfoVertex.offset = 0;
        storageBufInfoVertex.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo storageBufInfoIndex{};
        storageBufInfoIndex.buffer = indexBuffer;
        storageBufInfoIndex.offset = 0;
        storageBufInfoIndex.range = VK_WHOLE_SIZE; // take the entire buffer

        VkDescriptorBufferInfo storageBufInfoOffset{};
        storageBufInfoOffset.buffer = offsetBuffer;
        storageBufInfoOffset.offset = 0;
        storageBufInfoOffset.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo materialBufferInfo{};
        materialBufferInfo.buffer = materialBuffer;
        materialBufferInfo.range = VK_WHOLE_SIZE;
        materialBufferInfo.offset = 0;


        VkWriteDescriptorSet writeAccelStr{};
        writeAccelStr.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeAccelStr.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writeAccelStr.dstSet = descriptorSets[i];
        writeAccelStr.dstBinding = 0;
        writeAccelStr.descriptorCount = 1;
        writeAccelStr.dstArrayElement = 0;
        writeAccelStr.pNext = &accelStruct;

        VkWriteDescriptorSet writeStorageImage{};
        writeStorageImage.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeStorageImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeStorageImage.dstSet = descriptorSets[i];
        writeStorageImage.dstBinding = 1;
        writeStorageImage.descriptorCount = 1;
        writeStorageImage.dstArrayElement = 0;
        writeStorageImage.pImageInfo = &storageImageInfo;
        writeStorageImage.pNext = nullptr;

        VkWriteDescriptorSet writeDstImage{};
        writeDstImage.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDstImage.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeDstImage.dstSet = descriptorSets[i];
        writeDstImage.dstBinding = 2;
        writeDstImage.descriptorCount = 1;
        writeDstImage.dstArrayElement = 0;
        writeDstImage.pImageInfo = &dstImageInfo;
        writeDstImage.pNext = nullptr;

        VkWriteDescriptorSet writeUBO{};
        writeUBO.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUBO.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUBO.dstSet = descriptorSets[i];
        writeUBO.dstBinding = 3;
        writeUBO.descriptorCount = 1;
        writeUBO.dstArrayElement = 0;
        writeUBO.pBufferInfo = &UBOInfo;
        writeUBO.pNext = nullptr;

        VkWriteDescriptorSet writeStorageHitVertex{};
        writeStorageHitVertex.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeStorageHitVertex.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeStorageHitVertex.dstSet = descriptorSets[i];
        writeStorageHitVertex.dstBinding = 4;
        writeStorageHitVertex.descriptorCount = 1;
        writeStorageHitVertex.dstArrayElement = 0;
        writeStorageHitVertex.pBufferInfo = &storageBufInfoVertex;
        writeStorageHitVertex.pNext = nullptr;

        VkWriteDescriptorSet writeStorageHitIndex{};
        writeStorageHitIndex.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeStorageHitIndex.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeStorageHitIndex.dstSet = descriptorSets[i];
        writeStorageHitIndex.dstBinding = 5;
        writeStorageHitIndex.descriptorCount = 1;
        writeStorageHitIndex.dstArrayElement = 0;
        writeStorageHitIndex.pBufferInfo = &storageBufInfoIndex;
        writeStorageHitIndex.pNext = nullptr;

        VkWriteDescriptorSet writeStorageHitOffsets{};
        writeStorageHitOffsets.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeStorageHitOffsets.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeStorageHitOffsets.dstSet = descriptorSets[i];
        writeStorageHitOffsets.dstBinding = 6;
        writeStorageHitOffsets.descriptorCount = 1;
        writeStorageHitOffsets.dstArrayElement = 0;
        writeStorageHitOffsets.pBufferInfo = &storageBufInfoOffset;
        writeStorageHitOffsets.pNext = nullptr;

        VkWriteDescriptorSet writeTextures{};
        writeTextures.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeTextures.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeTextures.dstSet = descriptorSets[i];
        writeTextures.dstBinding = 7;
        //writeTextures.descriptorCount = static_cast<uint32_t>(textureImgInfos.size());
        assert(textureImgInfos.size() == NUM_TEXTURES);
        writeTextures.descriptorCount = NUM_TEXTURES;
        writeTextures.dstArrayElement = 0;
        writeTextures.pImageInfo = textureImgInfos.data();
        writeTextures.pBufferInfo = nullptr;
        writeTextures.pTexelBufferView = nullptr;
        writeTextures.pNext = nullptr;

        VkWriteDescriptorSet writeMaterials{};
        writeMaterials.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeMaterials.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeMaterials.dstSet = descriptorSets[i];
        writeMaterials.dstBinding = 8;
        writeMaterials.descriptorCount = 1;
        writeMaterials.dstArrayElement = 0;
        writeMaterials.pBufferInfo = &materialBufferInfo;
        writeMaterials.pNext = nullptr;

        // TODO: add sampling
        std::array<VkWriteDescriptorSet, 9> descriptorWrites;

        descriptorWrites[0] = writeAccelStr;
        descriptorWrites[1] = writeStorageImage;
        descriptorWrites[2] = writeDstImage;
        descriptorWrites[3] = writeUBO;
        descriptorWrites[4] = writeStorageHitVertex;
        descriptorWrites[5] = writeStorageHitIndex;
        descriptorWrites[6] = writeStorageHitOffsets;
        descriptorWrites[7] = writeTextures;
        descriptorWrites[8] = writeMaterials;

        vkUpdateDescriptorSets(
            device,
            descriptorWrites.size(),
            descriptorWrites.data(),
            0,
            nullptr
            );
    }
}

void Renderer::createDescriptorSetLayout_RT() {

    /* TODO: reorganize into two different descriptor sets, first for scene to hold geometry buffers,
       second for output image and the ubo*/
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
    { 0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR }, // accumulation image
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR }, // dst image that gets copied into swapchain
        { 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        // will only use closest hit right now
        { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // vertex
        { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}, // index
        {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        { 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4096, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR}
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;

    std::vector<VkDescriptorBindingFlags> flags(bindings.size(), 0);
    flags[7] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    bindingFlags.bindingCount = flags.size();
    bindingFlags.pBindingFlags = flags.data();


    VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutCreateInfo.pNext = nullptr;
    layoutCreateInfo.pBindings = bindings.data();
    layoutCreateInfo.bindingCount = bindings.size();
    layoutCreateInfo.pNext = &bindingFlags;

    if (vkCreateDescriptorSetLayout(device, &layoutCreateInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed creating ray tracing descriptor set layout") ;
    }
}


void Renderer::createDescriptorPool_RT() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR , MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT * 4096},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed creating ray tracing descriptor pool");
    }
}

void Renderer::createShaderBindingTable() {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    uint32_t handleSize = rtProps.shaderGroupHandleSize;
    uint32_t handleCount = 4;

    auto alignUp = [](size_t value, size_t alignment) -> uint32_t {
        return (value + alignment - 1) & ~(alignment -1);
    };

    uint32_t handleSizeAligned = alignUp(handleSize, rtProps.shaderGroupHandleAlignment);
    // reminder: the stride of the closest hit and miss regions will probably be computed seperately
    // in the future, there is only one entry in all regions atm
    uint64_t regionStride = alignUp(handleSizeAligned, rtProps.shaderGroupBaseAlignment);

    rgenRegion.stride = regionStride;
    rgenRegion.size = rgenRegion.stride;
    missRegion.stride = handleSizeAligned;
    missRegion.size = alignUp(2 * handleSizeAligned, rtProps.shaderGroupBaseAlignment);
    chitRegion.stride = handleSizeAligned;
    chitRegion.size = regionStride;

    uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);

    if (pfnGetRayTracingShaderGroupHandlesKHR(device, graphicsPipeline, 0, handleCount, dataSize, handles.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed fetching shader group handles");
    }

    VkDeviceSize sbtSize =  rgenRegion.size + chitRegion.size + missRegion.size;
    createBuffer(sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        sbtBuffer, sbtBufferMemory
        );

    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.pNext = nullptr;
    info.buffer = sbtBuffer;
    sbtAddress = vkGetBufferDeviceAddress(device, &info);
    rgenRegion.deviceAddress = sbtAddress;
    missRegion.deviceAddress = sbtAddress + rgenRegion.size;
    chitRegion.deviceAddress = sbtAddress + rgenRegion.size + missRegion.size;

    auto getHandle = [&] (uint32_t idx) -> unsigned char* {
        return (handles.data() + idx * handleSize);
    };

    void *data;
    vkMapMemory(device, sbtBufferMemory, 0, dataSize, 0, &data);
    auto *pSBTBuffer = reinterpret_cast<uint8_t*>(data);
    uint8_t* pData = nullptr;

    pData = pSBTBuffer;
    memcpy(pData, getHandle(0), handleSize);

    // miss region
    pData = pSBTBuffer + rgenRegion.size;
    memcpy(pData, getHandle(1), handleSize); // miss
    pData += missRegion.stride;
    memcpy(pData, getHandle(2), handleSize); // smiss

    // chit region
    pData = pSBTBuffer + rgenRegion.size + missRegion.size;
    memcpy(pData, getHandle(3), handleSize); // chit

    vkUnmapMemory(device, sbtBufferMemory);
}


std::vector<char> Renderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed opening file!");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(this->device, &createInfo, nullptr, &shaderModule)){
        throw std::runtime_error("failed to create shader module");
    }

    return shaderModule;
}

void Renderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool");
    }
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers");
    }
}


void Renderer::raytrace(VkCommandBuffer cmdBuf, uint32_t imageIndex) {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, graphicsPipeline);

    vkCmdBindDescriptorSets(
        cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout,
        0, 1, descriptorSets.data(),
        0, nullptr);

    PushConstants pc{};

    glm::mat4 proj = glm::perspective(glm::radians(45.0f),
        swapChainExtent.width / (float) swapChainExtent.height,
        0.1f, 50.0f);

    proj[1][1] *= -1.0f;


    pc.viewInverse = glm::inverse(camera.getViewMatrix());
    pc.projInverse = glm::inverse(proj);
    pc.cameraPos = camera.pos;

    pc.frameIndex = frameCount;
    //pc.clearColor = glm::vec4(255.0f/255.0f,  244.0f/255.0f, 229.0f/255.0f, 1.0f);
    pc.clearColor = glm::vec4(0.53f, 0.81f, 0.98f, 1.0f);
    pc.lightIntensity = 2.0f;
    pc.textureCount = NUM_TEXTURES;

    vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(PushConstants), &pc);

    VkStridedDeviceAddressRegionKHR callableSBT{};
    callableSBT.deviceAddress = 0;
    callableSBT.size = 0;
    callableSBT.stride = 0;

    VkImageSubresourceLayers subresourceLayers{};
    subresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceLayers.baseArrayLayer = 0;
    subresourceLayers.mipLevel = 0;
    subresourceLayers.layerCount = 1;

    VkExtent3D extent;
    extent.depth = 1;
    extent.height = swapChainExtent.height;
    extent.width = swapChainExtent.width;

    VkImageCopy imageCopy{};
    imageCopy.srcOffset = {0,0,0};
    imageCopy.dstOffset = { 0,0,0};
    imageCopy.srcSubresource = subresourceLayers;
    imageCopy.dstSubresource = subresourceLayers;
    imageCopy.extent = extent;

    pfnCmdTraceRaysKHR(
    cmdBuf, &rgenRegion, &missRegion, &chitRegion,
    &callableSBT, swapChainExtent.width, swapChainExtent.height, 1);

    // compute pipeline not needed since the 8 byte image will be written to in the rgen shader anyway
    // setup might come in handy later
    /*
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_RT);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_RT,
        0, 1, descriptorSets.data(), 0, nullptr);


    //with 8 being the local size x and y of the compute shader
    uint32_t x = ceil(swapChainExtent.width / 8);
    uint32_t y = ceil(swapChainExtent.height / 8);
    uint32_t z = 1;

    vkCmdDispatch(cmdBuf, x, y, z);
    */

    transitionImageLayout(dstImage_RT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cmdBuf);
    transitionImageLayout(swapChainImages[imageIndex], VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cmdBuf);

    vkCmdCopyImage(cmdBuf, dstImage_RT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    //transitionImageLayout(swapChainImages[imageIndex], VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, cmdBuf);
    transitionImageLayout(swapChainImages[imageIndex], VK_FORMAT_B8G8R8A8_SRGB,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  cmdBuf); // TODO: transition to this only if gui is enabled
    transitionImageLayout(dstImage_RT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, cmdBuf);
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    //renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(swapChainImages.size());
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightImages.resize(swapChainImages.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;


    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
            || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS
        ) {
            throw std::runtime_error("failed creating sync objects (frame)");
        }
    }

    for (size_t i = 0; i < renderFinishedSemaphores.size(); i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed creating renderFinished semaphore");
        }
    }
}

void Renderer::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

void Renderer::createMaterialBuffer() {
    std::byte *materialData = reinterpret_cast<std::byte*>(materials.data());
    const size_t materialSize = materials.size() * sizeof(Material);

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(materialSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    void* stagingData;
    vkMapMemory(device, stagingBufferMemory, 0, materialSize, 0, &stagingData);
    memcpy(stagingData, materialData, materialSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(
            materialSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            materialBuffer,
            materialBufferMemory
            );

    copyBuffer(stagingBuffer, materialBuffer, materialSize);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = materialBuffer;
    materialBufferAddress= pfnGetBufferDeviceAddressKHR(device, &addrInfo);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void Renderer::createTextureImage(std::string& fpath, Texture& texture) {
    int texW, texH, texChannels;
    std::string fullPath = fpath;
    stbi_uc *pixels = stbi_load(fullPath.c_str(), &texW, &texH, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texW * texH * 4;

    if (pixels == nullptr) {
        throw std::runtime_error("failed to load texture image");
    }

    VkBuffer stagingBuffer{};
    VkDeviceMemory stagingBufferMemory{};

    createBuffer(
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
        );

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(
        texW,
        texH,
        VK_SAMPLE_COUNT_1_BIT,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        texture.image,
        texture.memory
        );

    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, texture.image, static_cast<uint32_t>(texW), static_cast<uint32_t>(texH));
    transitionImageLayout(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);;
}

void Renderer::createTextureSampler(Texture& texture) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;


    if (vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
        throw std::runtime_error("failed creating texture sampler");
    }
}

void Renderer::createTextureImageView(Texture& texture) {
    texture.view = createImageView(texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed creating texture image view");
    }
    return imageView;
}

void Renderer::createImage(
    uint32_t width,
    uint32_t height,
    VkSampleCountFlagBits numSamples,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory) {

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = numSamples;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    endSingleTimeCommands(commandBuffer);
}

void Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmdBuf) {

    bool tag = false; // bit messy to do this but this function needs to be available in contexts where there is already
                        // a command buffer being submitted upwards in the call stack, such as raytrace()
    if (cmdBuf == VK_NULL_HANDLE) {
        cmdBuf = beginSingleTimeCommands();
        tag = true;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage yet hence top of pipeline
        destinationStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;

        sourceStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // presentations happens outside of the pipeline
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
    cmdBuf,
    sourceStage, destinationStage,
    0, 0,
    nullptr,
    0, nullptr,
    1, &barrier
    );


    if (tag) {
        endSingleTimeCommands(cmdBuf);
    }
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format");
}

VkFormat Renderer::findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool Renderer::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void Renderer::createStorageImage_RT() {
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        storageImage_RT, storageImageMemory_RT
        );

    storageImageView_RT = createImageView(storageImage_RT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);

    transitionImageLayout(storageImage_RT, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}

void Renderer::createDstImage_RT() {
    createImage(swapChainExtent.width, swapChainExtent.height,
        VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        dstImage_RT, dstImageMemory_RT
        );

    dstImageView_RT = createImageView(dstImage_RT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);

    transitionImageLayout(dstImage_RT, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
}


void Renderer::traverseNodes_GLTF(tinygltf::Model& model) {

    const tinygltf::Scene& scene =
        model.scenes[model.defaultScene >= 0 ? model.defaultScene : 0];

    for (int nodeIndex : scene.nodes) {
        traverseNode_GLTF(model, model.nodes[nodeIndex], glm::mat4(1.0f));
    }
}

void Renderer::traverseNode_GLTF(tinygltf::Model& model, tinygltf::Node& node, glm::mat4 parentTransform) {
    glm::mat4 local = getFinalMatrix_GLTF(node);
    glm::mat4 worldTransform = parentTransform * local;

    processNode_GLTF(model, node, worldTransform);

    for (const auto i : node.children) {
        traverseNode_GLTF(model, model.nodes[i], worldTransform);
    }
}

void Renderer::processNode_GLTF(tinygltf::Model& model, tinygltf::Node& node, glm::mat4 parentTransform) {
    if (node.mesh >= 0) {
        MeshInfo& mInfo = meshInfos[node.mesh];

        for (uint32_t i = 0; i < mInfo.primitiveCount; i++) {
            PrimitiveInfo& pi = primitiveInfos[mInfo.firstPrimitive + i];

            SceneInstance si;
            si.primitiveIndex = mInfo.firstPrimitive + i;
            si.transform = parentTransform;
            sceneInstances.push_back(si);

        }
    }
}

glm::mat4 Renderer::getFinalMatrix_GLTF(tinygltf::Node& node) {
    glm::vec3 t(0.0f);
    if (!node.translation.empty()) {
        t.x = node.translation[0];
        t.y = node.translation[1];
        t.z = node.translation[2];
    }
    glm::mat4 T = glm::translate(glm::mat4(1.0f), t);

    glm::quat r(1.0f, 0.0f, 0.0f, 0.0f);
    if (!node.rotation.empty()) {
        r.x = node.rotation[0];
        r.y = node.rotation[1];
        r.z = node.rotation[2];
        r.w = node.rotation[3];
    }

    glm::mat4 R = glm::mat4_cast(r);

    glm::vec3 s(1.0f);
    if (!node.scale.empty()) {
        s.x = static_cast<float>(node.scale[0]);
        s.y = static_cast<float>(node.scale[1]);
        s.z = static_cast<float>(node.scale[2]);
    }
    glm::mat4 S = glm::scale(glm::mat4(1.0f), s);

    return T * R * S;
}

Material Renderer::fetchMaterialInfo(const tinygltf::Material& mat, const std::vector<int>& map) {
    Material m{};
    m.baseColorTexture = -1;
    m.normalTexture = -1;
    m.metallicRoughnessTexture = -1;
    m.occlusionTexture = -1;
    m.emissiveTexture = -1;

    m.baseColorFactor = glm::vec4(1.0f);
    m.emissiveFactor = glm::vec4(0.0f);


    const auto& pbr = mat.pbrMetallicRoughness;

    if (pbr.baseColorTexture.index >= 0) {
        m.baseColorTexture = map[pbr.baseColorTexture.index];
    } else {
        m.baseColorTexture = -1;
    }

    if (pbr.baseColorFactor.size() == 4) {
        m.baseColorFactor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
    }

    m.metallicFactor = pbr.metallicFactor;
    m.roughnessFactor = pbr.roughnessFactor;

    if (mat.emissiveFactor.size() == 3) {
        m.emissiveFactor = glm::vec4((float)mat.emissiveFactor[0], (float)mat.emissiveFactor[1], (float)mat.emissiveFactor[2], 0.0f);
    }

    float transmissionFactor = 0.0f;
    if (mat.extensions.count("KHR_materials_transmission")) {
        auto &ext = mat.extensions.at("KHR_materials_transmission");
        transmissionFactor = static_cast<float>(ext.Get("transmissionFactor").GetNumberAsDouble());
    }

    m.transmissionFactor = transmissionFactor;

    m.normalTexture = mat.normalTexture.index >= 0 ? map[mat.normalTexture.index] : -1;
    m.occlusionTexture = mat.occlusionTexture.index >= 0 ? map[mat.occlusionTexture.index] : -1;
    m.emissiveTexture = mat.emissiveTexture.index >= 0 ? map[mat.emissiveTexture.index] : -1;

    return m;
}

void Renderer::createScene_GLTF() {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    std::string filename;
    std::string textureDir = scenes[sceneIndex];

    for (const auto& entry : std::filesystem::directory_iterator(scenes[sceneIndex])) {
        if (entry.path().extension() == ".gltf") { // get the first gltf file in the directory
            filename = entry.path();
            break;
        }
    }

    //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
        throw std::runtime_error("Warn: " + warn);
    }

    if (!err.empty()) {
        throw std::runtime_error("Err: " + err);
    }

    if (!ret) {
        throw std::runtime_error("Failed to parse glTF: " + filename);
    }

    std::vector<glm::uvec4> offs;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;

    std::vector<int> mapGltfTextureToVulkan(model.textures.size(), -1);

    for (size_t texIdx = 0; texIdx < model.textures.size(); ++texIdx) {
        int source = model.textures[texIdx].source;

        // no image attached
        if (source < 0 || source >= model.images.size()) {
            mapGltfTextureToVulkan[texIdx] = -1;
            continue;
        }

        const tinygltf::Image& img = model.images[source];

        TextureInput tinput{};
        //std::string path = "dragon/" + img.uri;
        //std::string path = "Sponza/glTF/" + img.uri;
        //std::string path = "sphereTest/" + img.uri;
        //std::string path = "cornell_box-_original/" + img.uri;
        tinput.path = textureDir + img.uri;

        if (img.uri.empty()) {
            mapGltfTextureToVulkan[texIdx] = -1;
            continue;
        }

        int textureIndex;
        if (textureCache.contains(tinput.path)) {
            // reuse existing texture
            textureIndex = textureCache[tinput.path];
        } else {
            // create new texture
            Texture texture;
            std::cout << tinput.path << std::endl;
            createTextureImage(tinput.path, texture);
            createTextureImageView(texture);
            createTextureSampler(texture);

            textures.push_back(texture);
            textureIndex = textures.size() - 1;
            textureCache.insert({tinput.path, textureIndex});
        }

        mapGltfTextureToVulkan[texIdx] = textureIndex;
    }
    NUM_TEXTURES = textures.size();

    for (const tinygltf::Material& m : model.materials) {
        materials.push_back(fetchMaterialInfo(m, mapGltfTextureToVulkan));
    }

    // retrieve counts
    for (const tinygltf::Mesh& mesh : model.meshes) {
        MeshInfo mi;
        mi.firstPrimitive = primitiveInfos.size();

        for (const tinygltf::Primitive& primitive : mesh.primitives) {
            auto itVert = primitive.attributes.find("POSITION");
            if (itVert == primitive.attributes.end()) continue;

            int materialIndex = primitive.material;
            if (materialIndex < 0 ) {
                materialIndex = 0;
            }

            const tinygltf::Accessor& accessorVert = model.accessors[itVert->second];
            const int accessorIndex = primitive.indices;

            const uint32_t baseVertex = vertexCount;
            const uint32_t baseIndex = indexCount;

            const uint32_t localVertexCount = static_cast<uint32_t>(accessorVert.count);
            vertexCount += localVertexCount;

            bool hasIndices = primitive.indices != -1;

            const uint32_t localIndexCount = hasIndices ? static_cast<uint32_t>(model.accessors[accessorIndex].count) : localVertexCount;

            indexCount += localIndexCount;

            //offs.push_back(glm::uvec4(baseVertex, baseIndex, materialIndex, 0));
            offs.push_back(glm::uvec4(baseVertex, baseIndex, materialIndex, 1.0f));

            PrimitiveInfo pi;
            pi.blas.vertexCount = localVertexCount;
            pi.blas.indexCount = localIndexCount;
            pi.blas.vertexOffset = baseVertex;
            pi.blas.indexOffset = baseIndex;
            pi.blas.vertexFormat = VERTEX_FORMAT;
            primitiveInfos.push_back(pi);
        }

        mi.primitiveCount = primitiveInfos.size() - mi.firstPrimitive;
        meshInfos.push_back(mi);
    }

    arenaInit(&vertexArena, vertexCount * sizeof(Vertex));
    arenaInit(&indexArena, indexCount * sizeof(INDEX_TYPE));
    arenaInit(&offsetArena, offs.size() * sizeof(glm::uvec4));

    assert(vertexArena.capacity != 0);
    assert(indexArena.capacity != 0);
    assert(offsetArena.capacity != 0);

    arenaAlloc(&vertexArena, vertexCount * sizeof(Vertex));
    arenaAlloc(&indexArena, indexCount * sizeof(INDEX_TYPE));
    arenaAlloc(&offsetArena, offs.size() * sizeof(glm::uvec4));

    memcpy(offsetArena.data, offs.data(), offs.size() * sizeof(glm::uvec4));

    // write to arenas
    uint32_t primitiveCount = 0;
    for (const tinygltf::Mesh& mesh : model.meshes) {
        for (const tinygltf::Primitive& primitive : mesh.primitives) {

            const tinygltf::Material& mat = model.materials[primitive.material];

            auto itPos = primitive.attributes.find("POSITION");
            auto itNormal = primitive.attributes.find("NORMAL");

            std::map<std::string, int>::const_iterator itUV;
            if (mat.pbrMetallicRoughness.baseColorTexture.texCoord == 0) {
                itUV = primitive.attributes.find("TEXCOORD_0");
            } else if (mat.pbrMetallicRoughness.baseColorTexture.texCoord == 1) {
                itUV = primitive.attributes.find("TEXCOORD_1");
            }

            auto itColor = primitive.attributes.find("COLOR_0");

            auto accessorIndices = primitive.indices;

            if (itPos == primitive.attributes.end()) {
#ifdef NDEBUG
                std::cout << "primitive has no position" << std::endl;
#endif
                continue;
            }
            if (itNormal == primitive.attributes.end()) {
                std::cout << "primitive has no normal" << std::endl;
                // compute normals  ....
            }

            const bool hasColor = (itColor != primitive.attributes.end());

            const tinygltf::Accessor& accessorPos = model.accessors[itPos->second];
            const tinygltf::Accessor& accessorNormal = model.accessors[itNormal->second];
            const tinygltf::Accessor& accessorIndex = model.accessors[accessorIndices];
            const tinygltf::Accessor& accessorUV = model.accessors[itUV->second];
            const tinygltf::Accessor* accessorColor = nullptr;

            const tinygltf::BufferView& bufferViewPos = model.bufferViews[accessorPos.bufferView];
            const tinygltf::BufferView& bufferViewNormal = model.bufferViews[accessorNormal.bufferView];
            const tinygltf::BufferView& bufferViewIndex = model.bufferViews[accessorIndex.bufferView];
            const tinygltf::BufferView& bufferViewUV = model.bufferViews[accessorUV.bufferView];
            const tinygltf::BufferView* bufferViewColor = nullptr;

            const tinygltf::Buffer& bufferPos = model.buffers[bufferViewPos.buffer];
            const tinygltf::Buffer& bufferNormal = model.buffers[bufferViewNormal.buffer];
            const tinygltf::Buffer& bufferIndex = model.buffers[bufferViewIndex.buffer];
            const tinygltf::Buffer& bufferUV = model.buffers[bufferViewUV.buffer];
            const tinygltf::Buffer* bufferColor = nullptr;

            if (hasColor) {
                accessorColor = &model.accessors[itColor->second];
                bufferViewColor = &model.bufferViews[accessorColor->bufferView];
                bufferColor = &model.buffers[bufferViewColor->buffer];
            };

            auto* vertices = reinterpret_cast<Vertex*>(vertexArena.data);
            auto* indices = reinterpret_cast<uint32_t*>(indexArena.data);

            uint32_t baseVertex = offs[primitiveCount].x;
            uint32_t baseIndex = offs[primitiveCount].y;

            for (uint32_t v = 0; v < accessorPos.count; v++) {
                glm::vec3 pos = getVec3FromAccessor(accessorPos, bufferViewPos, bufferPos, v);
                glm::vec3 normal = getVec3FromAccessor(accessorNormal, bufferViewNormal, bufferNormal, v);
                glm::vec2 uv = getVec2FromAccessor(accessorUV, bufferViewUV, bufferUV, v);
                glm::vec3 color = hasColor ? getVec3FromAccessor(*accessorColor, *bufferViewColor, *bufferColor, v) : glm::vec3(1.0f);
                Vertex vert{};
                vert.pos = pos;
                vert.normal = normal;
                vert.color = color;
                vert.texture = uv;

                vertices[baseVertex + v] = vert;
            }

            if (primitive.indices != -1) {
                for (uint32_t i = 0; i < accessorIndex.count; i++) {
                    uint32_t idx = getIndexFromAccessor(accessorIndex, bufferViewIndex, bufferIndex, i);
                    indices[baseIndex + i] = idx;
                }
            } else {
                for (uint32_t i = 0; i < accessorPos.count; i++) {
                    indices[baseIndex + i] = i;
                }
            }

            primitiveCount++;
        }
    }


    traverseNodes_GLTF(model);
}

glm::vec3 Renderer::getVec3FromAccessor(const tinygltf::Accessor &accessor, const tinygltf::BufferView &bufferView,
    const tinygltf::Buffer &buffer, const size_t index) {

    size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);

    size_t byteStride = bufferView.byteStride;
    if (byteStride == 0) {
        byteStride = componentSize * numComponents;
    }

    //size_t byteStride = bufferView.byteStride == 0 ? sizeof(float) * 3 : bufferView.byteStride;

    const auto* bufData = reinterpret_cast<const std::byte*>(buffer.data.data());

    const std::byte* basePtr = bufData + bufferView.byteOffset + accessor.byteOffset;

    const auto* vector = reinterpret_cast<const float*>(basePtr + index * byteStride);

    return glm::vec3(vector[0], vector[1], vector[2]);
}

glm::vec2 Renderer::getVec2FromAccessor(const tinygltf::Accessor& accessor, const tinygltf::BufferView &bufferView,
    const tinygltf::Buffer &buffer, const size_t index) {

    size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);

    size_t byteStride = bufferView.byteStride;
    if (byteStride == 0) {
        byteStride = componentSize * numComponents;
    }

    const auto* bufData = reinterpret_cast<const std::byte*>(buffer.data.data());

    const std::byte* basePtr = bufData + bufferView.byteOffset + accessor.byteOffset;

    const auto* vector = reinterpret_cast<const float*>(basePtr + index * byteStride);

    return glm::vec2(vector[0], vector[1]);
}

uint32_t Renderer::getIndexFromAccessor(const tinygltf::Accessor &accessor, const tinygltf::BufferView &bufferView,
    const tinygltf::Buffer &buffer, const size_t index) {

    //size_t byteStride = bufferView.byteStride == 0 ? sizeof(uint32_t) : bufferView.byteStride;
    size_t componentSize = tinygltf::GetComponentSizeInBytes(accessor.componentType);
    size_t numComponents = tinygltf::GetNumComponentsInType(accessor.type);

    size_t byteStride = bufferView.byteStride;
    if (byteStride == 0) {
        byteStride = componentSize * numComponents;
    }

    const auto* bufData = reinterpret_cast<const std::byte*>(buffer.data.data());

    const std::byte* basePtr = bufData + bufferView.byteOffset + accessor.byteOffset;

    const std::byte* elementPtr = basePtr + index * byteStride;

    switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return *reinterpret_cast<const uint8_t*>(elementPtr);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            return *reinterpret_cast<const uint16_t*>(elementPtr);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            return *reinterpret_cast<const uint32_t*>(elementPtr);

        default:
            std::abort();
    }
}

void Renderer::createBottomLevelAccelerationStructures() {
    for (auto& pi : primitiveInfos) {
        BlasInput& blasInput = pi.blas;
        blasInput.vertexAddress = vertexBufferAddress + sizeof(Vertex) * blasInput.vertexOffset;
        blasInput.indexAddress = indexBufferAddress + sizeof(INDEX_TYPE) * blasInput.indexOffset;
        //blasInput.vertexAddress = vertexBufferAddress;
        //blasInput.indexAddress = indexBufferAddress;

        Blas blasInstance {};
        blasInstance.create(device, blasInput, *this);
        assert(blasInstance.handle != VK_NULL_HANDLE);
        blasPool.push_back(blasInstance);
    }
}

void Renderer::createTopLevelAccelerationStructure() {

    std::vector<TlasInstance> tlasInstances;

    for (size_t i = 0; i < sceneInstances.size(); i++) {
        const SceneInstance& inst = sceneInstances[i];

        const Blas& blas = blasPool[inst.primitiveIndex];

        TlasInstance tinstance{};
        tinstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        glm::mat4 m4 = inst.transform;
        glm::mat3x4 m3x4(inst.transform);
        tinstance.transform = m3x4;

        bool flipped = glm::determinant(m4) < 0.0f;
        if (flipped) tinstance.flags |= VK_GEOMETRY_INSTANCE_TRIANGLE_FLIP_FACING_BIT_KHR;


        VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo{};
        blasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        blasAddressInfo.accelerationStructure = blas.handle;
        tinstance.blasDeviceAddress = pfnGetAccelerationStructureDeviceAddressKHR(device, &blasAddressInfo);
        tinstance.instanceCustomIndex = inst.primitiveIndex;
        tinstance.mask = 0xFF;
        tinstance.instanceShaderBindingTableRecordOffset = 0;
        tlasInstances.push_back(tinstance);

    }

    tlas.create(device, tlasInstances, *this);
}

// the reason why glm instead of just using VkTransformMatrixKHR is to avoid including vulkan.h in tlas.h
glm::mat3x4 Renderer::createTopLevelTransformMatrix(glm::vec3 pos) {
    glm::mat3x4 mat(0.0f);

    mat[0][0] = 1.0f;
    mat[1][1] = 1.0f;
    mat[2][2] = 1.0f;

    mat[0][3] = pos.x;
    mat[1][3] = pos.y;
    mat[2][3] = pos.z;

    return mat;
}

void Renderer::initImguiBackend() {
    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.Queue = graphicsQueue;
    initInfo.DescriptorPool = guiHandle->descPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = MAX_FRAMES_IN_FLIGHT;
    initInfo.PipelineInfoMain.RenderPass = imguiRenderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&initInfo);
}

void Renderer::createImguiRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;   // keep ray traced image
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    vkCreateRenderPass(device, &rpInfo, nullptr, &imguiRenderPass);
}

void Renderer::recreateScene_GLTF() {
    vkDeviceWaitIdle(device);
    vkDestroyBuffer(device, vertexBuffer, nullptr); vertexBuffer = VK_NULL_HANDLE;
    vkDestroyBuffer(device, indexBuffer, nullptr); indexBuffer = VK_NULL_HANDLE;
    vkDestroyBuffer(device, offsetBuffer, nullptr); offsetBuffer = VK_NULL_HANDLE;
    vkDestroyBuffer(device, materialBuffer, nullptr); materialBuffer = VK_NULL_HANDLE;

    vkFreeMemory(device, vertexBufferMemory,nullptr); vertexBufferMemory = VK_NULL_HANDLE;
    vkFreeMemory(device, indexBufferMemory,nullptr); indexBufferMemory = VK_NULL_HANDLE;
    vkFreeMemory(device, offsetBufferMemory,nullptr); offsetBufferMemory = VK_NULL_HANDLE;
    vkFreeMemory(device, materialBufferMemory, nullptr); materialBufferMemory = VK_NULL_HANDLE;

    arenaFree(&vertexArena);
    arenaFree(&indexArena);
    arenaFree(&offsetArena);

    for (auto& blasInstance : blasPool) {
        blasInstance.destroy(device);
    }
    blasPool.clear();
    tlas.destroy(device);

    for (auto& texture : textures) {
        texture.cleanup(device);
    }

    textures.clear();
    materials.clear();
    primitiveInfos.clear();
    sceneInstances.clear();
    textureCache.clear();
    meshInfos.clear();

    vkFreeDescriptorSets(device, descriptorPool, MAX_FRAMES_IN_FLIGHT, descriptorSets.data());

    camera.resetCamera();

    frameCount = 0;

    createScene_GLTF();
    createVertexBuffer();
    createIndexBuffer();
    createOffsetBuffer();
    createMaterialBuffer();
    createBottomLevelAccelerationStructures();
    createTopLevelAccelerationStructure();
    createDescriptorSet_RT();
}

void Renderer::initGui() {
    if (guiHandle != nullptr) {
        createImguiRenderPass();
        initImguiBackend();
    }
}


