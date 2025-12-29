#pragma once

#include "vulkan/vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <cstring>
#include <optional>
#include <set>
#include <fstream>
#include <chrono>
#include <tuple>

#include "blas.h"
#include "tlas.h"
#include "vertex.h"
#include "camera.h"
#include "arena.h"
#include "host_device.h"
#include "globals.h"

#include "blas.h"
#include "tiny_obj_loader.h"
#include "stb_image.h"
#include "tlas.h"


VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
    );

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
    );

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }

};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

typedef enum RenderingMode {
    RENDERING_MODE_RAY_TRACING = 1,
    RENDERING_MODE_RASTERISATION, // TODO: in the future
    RENDERING_MODE_WIREFRAME,
} RenderingMode;

class Renderer {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    uint32_t currentFrame = 0;

    GLFWwindow* window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

public:
    VkDevice device;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

private:
    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    VkPipeline graphicsPipeline;
    VkPipelineLayout computePipelineLayout_RT;
    VkPipeline computePipeline_RT;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkDeviceAddress vertexBufferAddress;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDeviceAddress indexBufferAddress;

    VkBuffer offsetBuffer;
    VkDeviceMemory offsetBufferMemory;
    VkDeviceAddress offsetBufferAddress;

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void*> uniformBuffersMapped;

    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> inFlightImages;


    std::vector<size_t> vertexCounts;
    std::vector<size_t> indexCounts;

    Arena vertexArena;
    Arena indexArena;
    Arena offsetArena;

    std::vector<BlasInput> blasData;
    std::vector<Blas> blasPool;

    Tlas tlas;

    // uninitialized for now
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;

    // uninitialized for ray tracing
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;

    // uninitialized for now
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    bool framebufferResized = false;

    RenderingMode renderingMode = RENDERING_MODE_RAY_TRACING;

    Camera camera;

    float lastFrameT = 0.0f;
    uint frameCount = 0;

    VkImageView storageImageView_RT;
    VkDeviceMemory storageImageMemory_RT;
    VkImage storageImage_RT;

    VkImageView dstImageView_RT;
    VkDeviceMemory dstImageMemory_RT;
    VkImage dstImage_RT;

    VkBuffer sbtBuffer;
    VkDeviceMemory sbtBufferMemory;
    VkDeviceAddress sbtAddress;

    VkStridedDeviceAddressRegionKHR rgenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR chitRegion{};

    void initWindow();
    void initVulkan();
    void mainLoop();
    void drawFrame();

public:
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

private:
    void cleanup();
    void createInstance();

    static void keyInputCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
    static void mouseInputCallback(GLFWwindow *window, double xpos, double ypos);

    void createSurface();

    std::vector<const char*> getRequiredExtensions();

    bool checkValidationLayerSupport();
    void setupDebugMessenger();
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void pickPhysicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    void createLogicalDevice();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void createSwapChain();
    void recreateSwapChain();
    void cleanupSwapChain();

    void createVertexBuffer();
    void createIndexBuffer();
    void createOffsetBuffer();

public:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    void createGraphicsPipeline();
    void createPipeline_RT();

    void createShaderBindingTable();

    void createUniformBuffers();
    void updateUniformBuffer(uint32_t currentImage);

    void createCamera();

    void createDescriptorPool();
    void createDescriptorSets();
    void createDescriptorSetLayout();

    void createDescriptorSet_RT();
    void createDescriptorPool_RT();
    void createDescriptorSetLayout_RT();

    static std::vector<char> readFile(const std::string& filename);

    VkShaderModule createShaderModule(const std::vector<char>& code);

    void createRenderPass();

    void createFrameBuffers();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t imageIndex);

    void raytrace(VkCommandBuffer cmdBuf, uint32_t imageIndex);

    void createSyncObjects();

    void createTextureImage();
    void createTextureSampler();
    void createTextureImageView();

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    void createImage(
        uint32_t width,
        uint32_t height,
        VkSampleCountFlagBits numSamples,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
        );
    void createImageViews();
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmdBuf = VK_NULL_HANDLE);

    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    void createDepthResources();
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    void createColorResources();

    void createStorageImage_RT();
    void createDstImage_RT();

    void createAccelerationStructures();
    void createBottomLevelAccelerationStructures();
    void createTopLevelAccelerationStructure();

    void loadMeshes();
    bool writeMeshData(const std::string& fpath, std::byte* vertexData, std::byte* indexData);
    glm::uvec2 fetchMeshMetadata(const std::string& fpath);
};

